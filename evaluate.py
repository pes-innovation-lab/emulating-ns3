#!/usr/bin/env python3
"""
Orchestration script for the Protocol Evaluation Bench.
Spins up real-world containers, runs ns-3 simulations, parses metrics,
calculates standard deviation and z-score differences, and outputs a scoring report.
"""
import os
import sys
import time
import datetime
import argparse
import numpy as np
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend
import matplotlib.pyplot as plt
import json
import tomllib
import docker

from parsers import parse_iperf3, parse_perfdhcp, parse_arping, parse_ns3_output

def ensure_image(client, image_name):
    """Checks if the image exists locally, and pulls it if not."""
    try:
        client.images.get(image_name)
    except docker.errors.ImageNotFound:
        print(f"Image '{image_name}' not found locally. Pulling...")
        client.images.pull(image_name)

def clean_leftover_processes(container, protocol):
    """Kills any leftover processes from previous runs inside the container."""
    if protocol in ['tcp', 'udp']:
        container.exec_run("pkill -9 iperf3", user="root")
    elif protocol == 'dhcp':
        container.exec_run("pkill -9 dnsmasq", user="root")
        container.exec_run("pkill -9 perfdhcp", user="root")
    elif protocol == 'arp':
        container.exec_run("pkill -9 arping", user="root")

def run_real_world(client, protocol, config, num_runs, timeout, output_dir):
    """Runs the real-world baseline containers using pair-ns-3 driver."""
    print("\n--- Part 1: Running Real-World baseline ---")
    p_config = config['protocols'][protocol]
    
    # Define network options
    net_name = f"eval_net_{protocol}"
    
    # 1. Setup Docker network with pair-ns-3 driver
    print(f"Creating Docker network '{net_name}' with pair-ns-3 driver...")
    try:
        network = client.networks.get(net_name)
        network.remove()
        time.sleep(1)
    except docker.errors.NotFound:
        pass
        
    network = client.networks.create(
        name=net_name,
        driver="pair-ns-3:latest",
        options={"if-prefix": "nk", "type": "netkit-l2"},
        ipam=docker.types.IPAMConfig(
            pool_configs=[
                docker.types.IPAMPool(
                    subnet="10.10.0.0/24",
                    gateway="10.10.0.254"
                )
            ]
        )
    )
    
    server_container = None
    client_container = None
    
    try:
        # Pull images if not present
        ensure_image(client, p_config['server_image'])
        ensure_image(client, p_config['client_image'])
        
        # 2. Start server container (10.10.0.1)
        server_name = f"eval_server_{protocol}"
        print(f"Starting server container '{server_name}'...")
        try:
            c = client.containers.get(server_name)
            c.remove(force=True)
        except docker.errors.NotFound:
            pass
            
        server_container = client.containers.create(
            image=p_config['server_image'],
            command=["tail", "-f", "/dev/null"],
            name=server_name,
            detach=True,
            cap_add=["NET_ADMIN", "NET_RAW"]
        )
        server_container.start()
        
        # 3. Start client container (10.10.0.2)
        client_name = f"eval_client_{protocol}"
        print(f"Starting client container '{client_name}'...")
        try:
            c = client.containers.get(client_name)
            c.remove(force=True)
        except docker.errors.NotFound:
            pass
            
        client_container = client.containers.create(
            image=p_config['client_image'],
            command=["tail", "-f", "/dev/null"],
            name=client_name,
            detach=True,
            cap_add=["NET_ADMIN", "NET_RAW"]
        )
        client_container.start()
        
        # 4. Install packages dynamically (while connected to default bridge with internet)
        print("Dynamically installing packages inside server container...")
        server_container.exec_run("apt-get update", user="root")
        if p_config['server_packages']:
            install_cmd = "apt-get install -y " + " ".join(p_config['server_packages'])
            code, out = server_container.exec_run(install_cmd, user="root")
            if code != 0:
                print(f"Server packages installation failed: {out.decode()}")
                
        print("Dynamically installing packages inside client container...")
        client_container.exec_run("apt-get update", user="root")
        if p_config['client_packages']:
            install_cmd = "apt-get install -y " + " ".join(p_config['client_packages'])
            code, out = client_container.exec_run(install_cmd, user="root")
            if code != 0:
                print(f"Client packages installation failed: {out.decode()}")

        # 5. Connect both containers to the private evaluation network
        print("Connecting containers to evaluation network...")
        network.connect(server_container, ipv4_address="10.10.0.1")
        network.connect(client_container, ipv4_address="10.10.0.2")
        
        metrics = []
        raw_outputs = []
        
        # 5. Run iterations
        for run in range(1, num_runs + 1):
            print(f"Run {run}/{num_runs}...")
            clean_leftover_processes(server_container, protocol)
            clean_leftover_processes(client_container, protocol)
            time.sleep(1)
            
            # Start server command in background
            server_cmd = p_config['server_cmd']
            print(f"  Starting server: {server_cmd}")
            server_container.exec_run(server_cmd, detach=True, user="root")
            time.sleep(2)  # Wait for server to bind
            
            # Run client command synchronously
            client_cmd = p_config['client_cmd']
            print(f"  Running client: {client_cmd}")
            start_time = time.time()
            code, out = client_container.exec_run(client_cmd, user="root")
            duration = time.time() - start_time
            
            stdout_text = out.decode("utf-8", errors="ignore")
            raw_outputs.append(stdout_text)
            
            # Parse result
            val = 0.0
            if protocol in ['tcp', 'udp']:
                res = parse_iperf3(stdout_text)
                val = res[p_config['metric_name']]
            elif protocol == 'dhcp':
                res = parse_perfdhcp(stdout_text)
                val = res[p_config['metric_name']]
            elif protocol == 'arp':
                res = parse_arping(stdout_text)
                val = res[p_config['metric_name']]
                
            print(f"  Result metric ({p_config['metric_name']}): {val} {p_config['metric_unit']} (Code: {code}, Duration: {duration:.2f}s)")
            metrics.append(val)
            
        return metrics, raw_outputs
        
    finally:
        # Cleanup Part 1
        print("Cleaning up real-world containers and network...")
        if client_container:
            try:
                client_container.remove(force=True)
            except Exception:
                pass
        if server_container:
            try:
                server_container.remove(force=True)
            except Exception:
                pass
        if network:
            try:
                network.remove()
            except Exception:
                pass

def run_ns3_simulation(client, protocol, config, num_runs, timeout, output_dir):
    """Runs the ns-3 simulation baseline."""
    print("\n--- Part 2: Running ns-3 Simulation baseline ---")
    p_config = config['protocols'][protocol]
    ns3_script = p_config['ns3_script']
    
    # Ensure image exists or build it
    image_tag = "ns3-eval-sim:latest"
    try:
        client.images.get(image_tag)
    except docker.errors.ImageNotFound:
        print(f"Image {image_tag} not found. Building it from ns3-node/Dockerfile...")
        client.images.build(
            path="./ns3-node",
            tag=image_tag,
            buildargs={"BUILD_PROF": "debug"}
        )
        
    metrics = []
    raw_outputs = []
    
    # Absolute paths for bind mounts
    scratch_dir = os.path.abspath("./ns3-node/scratch")
    run_sim_script = os.path.abspath("./ns3-node/run_sim.sh")
    narrow_script = os.path.abspath("./ns3-node/narrow-noarp-interfaces.sh")
    
    sim_container = None
    try:
        print("Starting persistent ns-3 simulation container...")
        sim_container = client.containers.run(
            image=image_tag,
            command=["tail", "-f", "/dev/null"],
            volumes={
                scratch_dir: {"bind": "/app/ns-3/scratch/scripts", "mode": "rw"},
                run_sim_script: {"bind": "/app/ns-3/run_sim.sh", "mode": "ro"},
                narrow_script: {"bind": "/app/ns-3/narrow-noarp-interfaces.sh", "mode": "ro"}
            },
            environment={"BUILD_PROF": "debug"},
            cap_add=["NET_ADMIN", "NET_RAW"],
            detach=True
        )
        
        # Give container a second to start
        time.sleep(1)
        
        for run in range(1, num_runs + 1):
            print(f"Run {run}/{num_runs}...")
            
            start_time = time.time()
            code, out = sim_container.exec_run(
                ["/app/ns-3/run_sim.sh", ns3_script],
                user="root"
            )
            duration = time.time() - start_time
            
            stdout_text = out.decode("utf-8", errors="ignore")
            raw_outputs.append(stdout_text)
            
            # Parse result
            res = parse_ns3_output(stdout_text)
            val = res[p_config['metric_name']]
            print(f"  Result metric ({p_config['metric_name']}): {val} {p_config['metric_unit']} (Exit Code: {code}, Duration: {duration:.2f}s)")
            metrics.append(val)
            
    finally:
        if sim_container:
            print("Cleaning up ns-3 simulation container...")
            try:
                sim_container.remove(force=True)
            except Exception:
                pass
                
    return metrics, raw_outputs

def calculate_score(z_score, scoring_table):
    """Calculates score out of 10 based on z-score and scoring table."""
    # Sort scoring table by z-score threshold ascending
    sorted_table = sorted(scoring_table, key=lambda x: x[0])
    for threshold, score in sorted_table:
        if z_score <= threshold:
            return score
    return 0

def generate_report(protocol, p_config, real_metrics, sim_metrics, z_score, score, output_dir):
    """Generates Markdown, JSON reports and plots the comparison graph."""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs(output_dir, exist_ok=True)
    
    # Calculate stats
    mean_real = np.mean(real_metrics)
    std_real = np.std(real_metrics)
    mean_sim = np.mean(sim_metrics)
    std_sim = np.std(sim_metrics)
    
    # 1. Save JSON data
    data = {
        "protocol": protocol,
        "metric_name": p_config['metric_name'],
        "metric_unit": p_config['metric_unit'],
        "timestamp": timestamp,
        "runs": len(real_metrics),
        "real_world": {
            "runs": real_metrics,
            "mean": float(mean_real),
            "std_dev": float(std_real)
        },
        "simulation": {
            "runs": sim_metrics,
            "mean": float(mean_sim),
            "std_dev": float(std_sim)
        },
        "z_score": float(z_score),
        "score": int(score)
    }
    
    json_path = os.path.join(output_dir, f"eval_data_{protocol}_{timestamp}.json")
    with open(json_path, "w") as f:
        json.dump(data, f, indent=4)
    print(f"Saved raw JSON data to: {json_path}")
    
    # 2. Save Matplotlib Plot
    plot_path = os.path.join(output_dir, f"eval_plot_{protocol}_{timestamp}.png")
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    # Bar plot with standard deviation error bars
    ax1.bar(["Real-World", "ns-3 Sim"], [mean_real, mean_sim], yerr=[std_real, std_sim], 
            capsize=10, color=["#3498db", "#2ecc71"], edgecolor="grey", alpha=0.8)
    ax1.set_ylabel(f"{p_config['metric_name'].capitalize()} ({p_config['metric_unit']})")
    ax1.set_title(f"Mean {p_config['metric_name'].capitalize()} Comparison")
    ax1.grid(axis='y', linestyle='--', alpha=0.7)
    
    # Box plot of the distributions
    ax2.boxplot([real_metrics, sim_metrics])
    ax2.set_xticklabels(["Real-World", "ns-3 Sim"])
    ax2.set_ylabel(f"{p_config['metric_name'].capitalize()} ({p_config['metric_unit']})")
    ax2.set_title("Metric Distribution (Boxplot)")
    ax2.grid(linestyle='--', alpha=0.7)
    
    fig.suptitle(f"{p_config['name']} Evaluation: Real-World vs ns-3 Sim\nFinal Score: {score}/10 (z-score: {z_score:.4f})")
    plt.tight_layout()
    plt.savefig(plot_path)
    plt.close()
    print(f"Saved comparison plot to: {plot_path}")
    
    # 3. Save Markdown Report
    md_path = os.path.join(output_dir, f"eval_report_{protocol}_{timestamp}.md")
    
    runs_table = ""
    for idx, (r, s) in enumerate(zip(real_metrics, sim_metrics), 1):
        runs_table += f"| Run {idx} | {r:.4f} | {s:.4f} | {abs(r - s):.4f} |\n"
        
    md_content = f"""# Protocol Evaluation Report: {p_config['name']}

**Timestamp:** {datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
**Final Evaluation Score:** {score}/10

## Metrics Comparison

Target Metric: **{p_config['metric_name']}** ({p_config['metric_unit']})

| Run Metric | Real-World | ns-3 Sim | Difference |
| :--- | :--- | :--- | :--- |
| **Mean** | {mean_real:.4f} | {mean_sim:.4f} | {abs(mean_real - mean_sim):.4f} |
| **Std Dev** | {std_real:.4f} | {std_sim:.4f} | - |

### Run-by-Run Values

| Run # | Real-World ({p_config['metric_unit']}) | ns-3 Sim ({p_config['metric_unit']}) | Absolute Diff ({p_config['metric_unit']}) |
| :--- | :--- | :--- | :--- |
{runs_table}

## Statistical Significance & Scoring

- **Z-Score Difference:** `{z_score:.4f}`
  *(Calculated as: `|mean_sim - mean_real| / std_dev_real`)*
- **Score:** `{score}/10`

### Stepwise Scoring Configuration Used:
| Max Z-Score Difference | Resulting Score |
| :--- | :--- |
"""
    for threshold, scr in sorted(p_config['scoring_table'], key=lambda x: x[0]):
        mark = " (Selected)" if z_score <= threshold and score == scr else ""
        md_content += f"| $\\le$ {threshold} | {scr}/10{mark} |\n"
        
    md_content += f"""
## Visualization

![Comparison Plot]({os.path.basename(plot_path)})
"""
    with open(md_path, "w") as f:
        f.write(md_content)
    print(f"Saved Markdown report to: {md_path}")
    
    # Print terminal output summary
    print("\n" + "="*50)
    print(f"EVALUATION SUMMARY: {p_config['name']}")
    print("="*50)
    print(f"Real-World Mean: {mean_real:.4f} ± {std_real:.4f} {p_config['metric_unit']}")
    print(f"ns-3 Sim Mean:   {mean_sim:.4f} ± {std_sim:.4f} {p_config['metric_unit']}")
    print(f"Z-Score Diff:    {z_score:.4f}")
    print(f"Final Score:     {score}/10")
    print("="*50 + "\n")

def main():
    parser = argparse.ArgumentParser(description="ns-3 Conformance Evaluation Bench")
    parser.add_argument("--config", default="config.toml", help="Path to config.toml")
    parser.add_argument("--protocol", required=True, choices=["tcp", "udp", "dhcp", "arp"], help="Protocol to evaluate")
    parser.add_argument("--runs", type=int, help="Override number of runs")
    parser.add_argument("--timeout", type=int, help="Override run timeout (seconds)")
    args = parser.parse_args()
    
    if not os.path.exists(args.config):
        print(f"Error: Configuration file '{args.config}' not found.")
        sys.exit(1)
        
    with open(args.config, "rb") as f:
        config = tomllib.load(f)
        
    protocol = args.protocol
    p_config = config['protocols'][protocol]
    
    num_runs = args.runs or config['global'].get('number_of_runs', 5)
    timeout = args.timeout or config['global'].get('timeout', 60)
    output_dir = config['global'].get('output_dir', "./eval_results")
    
    client = docker.from_env()
    
    # Run Real-World tests
    real_metrics, real_raw = run_real_world(client, protocol, config, num_runs, timeout, output_dir)
    
    # Run Simulation tests
    sim_metrics, sim_raw = run_ns3_simulation(client, protocol, config, num_runs, timeout, output_dir)
    
    # Calculate difference metrics
    mean_real = np.mean(real_metrics)
    std_real = np.std(real_metrics)
    mean_sim = np.mean(sim_metrics)
    
    if std_real == 0.0:
        if mean_real == mean_sim:
            z_score = 0.0
        else:
            # Handle standard deviation of zero by using epsilon
            z_score = abs(mean_sim - mean_real) / 1e-6
    else:
        z_score = abs(mean_sim - mean_real) / std_real
        
    score = calculate_score(z_score, p_config['scoring_table'])
    
    # Generate report and plot
    generate_report(protocol, p_config, real_metrics, sim_metrics, z_score, score, output_dir)

if __name__ == "__main__":
    main()
