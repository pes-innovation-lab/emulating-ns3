#!/usr/bin/env python3
"""
Orchestration script for the Protocol Evaluation Bench.
Spins up real-world containers, runs ns-3 simulations, parses metrics,
calculates standard deviation and z-score differences, and outputs a scoring report.

Protocol behaviour (packages, commands, cleanup, parser, scored metrics) all
comes from config.toml - adding a protocol needs no changes here.
"""

import argparse
import datetime
import os
import sys
import time

import matplotlib
import numpy as np

matplotlib.use("Agg")  # Non-interactive backend
import json
import tomllib
from concurrent.futures import ThreadPoolExecutor

import docker
import matplotlib.pyplot as plt

from parsers import PARSERS, parse_ns3_output


def run_pre_install_cmds(container, cmds, label):
    """
    Runs arbitrary shell commands inside a container before package
    installation.
    """
    for cmd in cmds:
        print(f"Running pre-install command inside {label} container: {cmd}")
        code, out = container.exec_run(["bash", "-c", cmd], user="root")
        if code != 0:
            print(f"Pre-install command failed ({label}): {out.decode()}")


def provision_container(container, p_config, side):
    """Adds any vendor repos and installs packages inside one container (server or client).

    Raises RuntimeError on apt-get failure instead of continuing silently -
    a missing traffic-generator binary would otherwise fail every run and
    get absorbed as a silent N/A instead of a clear setup error.
    """
    label = side
    run_pre_install_cmds(container, p_config.get(f"{side}_pre_install_cmds", []), label)
    print(f"Dynamically installing packages inside {label} container...")
    code, out = container.exec_run("apt-get update", user="root")
    if code != 0:
        raise RuntimeError(
            f"{label} apt-get update failed: {out.decode(errors='ignore')}"
        )
    packages = p_config.get(f"{side}_packages", [])
    if packages:
        install_cmd = "apt-get install -y " + " ".join(packages)
        code, out = container.exec_run(install_cmd, user="root")
        if code != 0:
            raise RuntimeError(
                f"{label} package installation failed: {out.decode(errors='ignore')}"
            )


def ensure_image(client, image_name):
    """Checks if the image exists locally, and pulls it if not."""
    try:
        client.images.get(image_name)
    except docker.errors.ImageNotFound:
        print(f"Image '{image_name}' not found locally. Pulling...")
        client.images.pull(image_name)


def clean_leftover_processes(container, p_config):
    """Kills any leftover processes from previous runs inside the container."""
    for cmd in p_config.get("cleanup_cmds", []):
        container.exec_run(cmd, user="root")


def run_real_world(client, protocol, config, num_runs, timeout, output_dir):
    """Runs the real-world baseline containers using pair-ns-3 driver."""
    print("\n--- Part 1: Running Real-World baseline ---")
    p_config = config["protocols"][protocol]
    parser_fn = PARSERS[p_config["parser"]]

    net_name = f"eval_net_{protocol}"

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
                docker.types.IPAMPool(subnet="10.10.0.0/24", gateway="10.10.0.254")
            ]
        ),
    )

    server_container = None
    client_container = None

    try:
        ensure_image(client, p_config["server_image"])
        ensure_image(client, p_config["client_image"])

        server_name = f"eval_server_{protocol}"
        print(f"Starting server container '{server_name}'...")
        try:
            c = client.containers.get(server_name)
            c.remove(force=True)
        except docker.errors.NotFound:
            pass

        server_container = client.containers.create(
            image=p_config["server_image"],
            command=["tail", "-f", "/dev/null"],
            name=server_name,
            detach=True,
            cap_add=["NET_ADMIN", "NET_RAW"],
        )
        server_container.start()

        client_name = f"eval_client_{protocol}"
        print(f"Starting client container '{client_name}'...")
        try:
            c = client.containers.get(client_name)
            c.remove(force=True)
        except docker.errors.NotFound:
            pass

        client_container = client.containers.create(
            image=p_config["client_image"],
            command=["tail", "-f", "/dev/null"],
            name=client_name,
            detach=True,
            cap_add=["NET_ADMIN", "NET_RAW"],
        )
        client_container.start()

        # Server/client provisioning is independent - run concurrently instead
        # of paying apt-get's update+install latency twice, back to back.
        with ThreadPoolExecutor(max_workers=2) as pool:
            list(
                pool.map(
                    lambda args: provision_container(*args),
                    [
                        (server_container, p_config, "server"),
                        (client_container, p_config, "client"),
                    ],
                )
            )

        print("Connecting containers to evaluation network...")
        network.connect(server_container, ipv4_address="10.10.0.1")
        network.connect(client_container, ipv4_address="10.10.0.2")

        metrics_per_run = []  # list of dicts, one per run
        raw_outputs = []

        for run in range(1, num_runs + 1):
            print(f"Run {run}/{num_runs}...")
            clean_leftover_processes(server_container, p_config)
            clean_leftover_processes(client_container, p_config)
            time.sleep(1)

            server_cmd = p_config["server_cmd"]
            client_cmd = p_config["client_cmd"]
            timed_client_cmd = ["bash", "-c", f"timeout -k 5 {timeout}s {client_cmd}"]

            code, stdout_text, duration = -1, "", 0.0
            max_attempts = 3
            for attempt in range(1, max_attempts + 1):
                # server_cmd is often one-shot (e.g. `iperf3 -s -1`) - restart
                # it every attempt, not just once, or retries hit a dead server.
                if attempt > 1:
                    clean_leftover_processes(server_container, p_config)
                    clean_leftover_processes(client_container, p_config)
                    time.sleep(1)
                print(f"  Starting server: {server_cmd}")
                try:
                    server_container.exec_run(server_cmd, detach=True, user="root")
                except docker.errors.APIError as e:
                    print(f"  Server command failed to start: {e}")
                time.sleep(2)  # Wait for server to bind

                print(
                    f"  Running client (attempt {attempt}/{max_attempts}): {client_cmd}"
                )
                start_time = time.time()
                try:
                    code, out = client_container.exec_run(timed_client_cmd, user="root")
                    stdout_text = out.decode("utf-8", errors="ignore")
                except docker.errors.APIError as e:
                    print(f"  Client command failed to run: {e}")
                    code, stdout_text = -1, ""
                duration = time.time() - start_time
                if code == 0:
                    break
                if code == 124:
                    print(f"  Client command timed out after {timeout}s")
                else:
                    print(f"  Client command exited {code}, retrying")

            if code != 0:
                print(
                    f"  Run {run} FAILED after {max_attempts} attempts (last exit code {code})"
                )
                # Discard rather than parse - partial output can still match
                # a metric's regex and pollute the mean with a bogus value.
                stdout_text = ""

            raw_outputs.append(stdout_text)

            res = parser_fn(stdout_text) if code == 0 else {}
            print(f"  Parsed metrics: {res} (Code: {code}, Duration: {duration:.2f}s)")
            metrics_per_run.append(res)

        return metrics_per_run, raw_outputs

    finally:
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
    p_config = config["protocols"][protocol]
    ns3_script = p_config["ns3_script"]

    image_tag = "ns3-eval-sim:latest"

    build_prof = config["global"].get("ns3_build_profile", "optimized")

    try:
        client.images.get(image_tag)
    except docker.errors.ImageNotFound:
        print(f"Image {image_tag} not found. Building it from ns3-node/Dockerfile...")
        client.images.build(
            path="./ns3-node", tag=image_tag, buildargs={"BUILD_PROF": build_prof}
        )

    metrics_per_run = []
    raw_outputs = []

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
                narrow_script: {
                    "bind": "/app/ns-3/narrow-noarp-interfaces.sh",
                    "mode": "ro",
                },
            },
            environment={"BUILD_PROF": build_prof},
            cap_add=["NET_ADMIN", "NET_RAW"],
            detach=True,
        )

        time.sleep(1)

        # run_sim.sh reconfigures with --enable-examples --enable-tests every
        # call, forcing a full rebuild on a fresh container - give it a floor
        # so a short `timeout` can't SIGKILL a legitimate first-run build.
        ns3_timeout = max(timeout, 300)

        for run in range(1, num_runs + 1):
            print(f"Run {run}/{num_runs}...")

            start_time = time.time()
            try:
                # NS_GLOBAL_VALUE seeds RngRun per run without touching
                # run_sim.sh, which other callers also use.
                code, out = sim_container.exec_run(
                    [
                        "bash",
                        "-c",
                        f"timeout -k 5 {ns3_timeout}s /app/ns-3/run_sim.sh {ns3_script}",
                    ],
                    user="root",
                    environment={"NS_GLOBAL_VALUE": f"RngRun={run}"},
                )
                stdout_text = out.decode("utf-8", errors="ignore")
            except docker.errors.APIError as e:
                print(f"  ns-3 run failed: {e}")
                code, stdout_text = -1, ""
            if code == 124:
                print(f"  ns-3 run timed out after {ns3_timeout}s")
            duration = time.time() - start_time
            raw_outputs.append(stdout_text)

            res = parse_ns3_output(stdout_text)
            print(
                f"  Parsed metrics: {res} (Exit Code: {code}, Duration: {duration:.2f}s)"
            )
            metrics_per_run.append(res)

    finally:
        if sim_container:
            print("Cleaning up ns-3 simulation container...")
            try:
                sim_container.remove(force=True)
            except Exception:
                pass

    return metrics_per_run, raw_outputs


def calculate_score(z_score, scoring_table):
    """Calculates score out of 10 based on z-score and scoring table."""
    sorted_table = sorted(scoring_table, key=lambda x: x[0])
    for threshold, score in sorted_table:
        if z_score <= threshold:
            return score
    return 0


def score_metrics(p_config, real_metrics_per_run, sim_metrics_per_run, global_cfg=None):
    """
    Scores every metric present in at least `min_run_coverage` fraction of
    runs on both sides (default 80%, [global] in config.toml); below that
    it's N/A rather than 0 - a missing measurement isn't a zero one.

    Z-score divides by a pooled std (real+sim) floored at 0.5% of the
    real-world mean, rather than std_real alone - real-world runs on a
    localhost docker network have near-zero natural variance, so std_real
    alone let 4th-decimal-place noise blow up the z-score.

    Returns:
        dict: {metric_name: {"available": bool, "real": [...], "sim": [...],
                              "mean_real", "std_real", "mean_sim", "std_sim",
                              "z_score", "score", "unit", "coverage"}}
        float or None: overall score (mean of available per-metric scores)
    """
    global_cfg = global_cfg or {}
    min_coverage = global_cfg.get("min_run_coverage", 0.8)

    results = {}
    for metric_cfg in p_config.get("metrics", []):
        name = metric_cfg["name"]
        real_vals = [r[name] for r in real_metrics_per_run if name in r]
        sim_vals = [s[name] for s in sim_metrics_per_run if name in s]

        n_real = len(real_metrics_per_run)
        n_sim = len(sim_metrics_per_run)
        real_coverage = len(real_vals) / n_real if n_real else 0.0
        sim_coverage = len(sim_vals) / n_sim if n_sim else 0.0
        coverage_str = f"{len(real_vals)}/{n_real} real, {len(sim_vals)}/{n_sim} sim"

        available = (
            len(real_vals) > 0
            and len(sim_vals) > 0
            and real_coverage >= min_coverage
            and sim_coverage >= min_coverage
        )

        if not available:
            results[name] = {
                "available": False,
                "unit": metric_cfg.get("unit", ""),
                "coverage": coverage_str,
            }
            continue

        mean_real = float(np.mean(real_vals))
        std_real = float(np.std(real_vals))
        mean_sim = float(np.mean(sim_vals))
        std_sim = float(np.std(sim_vals))

        pooled_std = float(np.sqrt((std_real**2 + std_sim**2) / 2))
        epsilon_floor = max(1e-9, 0.005 * abs(mean_real))
        denom = max(pooled_std, epsilon_floor)
        z_score = abs(mean_sim - mean_real) / denom

        score = calculate_score(z_score, metric_cfg["scoring_table"])

        results[name] = {
            "available": True,
            "unit": metric_cfg.get("unit", ""),
            "coverage": coverage_str,
            "real": real_vals,
            "sim": sim_vals,
            "mean_real": mean_real,
            "std_real": std_real,
            "mean_sim": mean_sim,
            "std_sim": std_sim,
            "z_score": z_score,
            "score": score,
            "scoring_table": metric_cfg["scoring_table"],
        }

    available_scores = [m["score"] for m in results.values() if m["available"]]
    overall_score = float(np.mean(available_scores)) if available_scores else None
    return results, overall_score


def generate_report(
    protocol, p_config, metric_results, overall_score, output_dir, min_run_coverage=0.8
):
    """Generates Markdown, JSON reports and plots the comparison graph, one panel per available metric."""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs(output_dir, exist_ok=True)

    available = {k: v for k, v in metric_results.items() if v["available"]}
    unavailable = [k for k, v in metric_results.items() if not v["available"]]

    # 1. Save JSON data
    data = {
        "protocol": protocol,
        "timestamp": timestamp,
        "metrics": metric_results,
        "overall_score": overall_score,
    }
    json_path = os.path.join(output_dir, f"eval_data_{protocol}_{timestamp}.json")
    with open(json_path, "w") as f:
        json.dump(data, f, indent=4)
    print(f"Saved raw JSON data to: {json_path}")

    # 2. Save Matplotlib Plot: one bar+box pair of columns per available metric
    plot_path = os.path.join(output_dir, f"eval_plot_{protocol}_{timestamp}.png")
    n = max(len(available), 1)
    fig, axes = plt.subplots(2, n, figsize=(6 * n, 9), squeeze=False)
    for idx, (name, m) in enumerate(available.items()):
        ax1, ax2 = axes[0][idx], axes[1][idx]
        ax1.bar(
            ["Real-World", "ns-3 Sim"],
            [m["mean_real"], m["mean_sim"]],
            yerr=[m["std_real"], m["std_sim"]],
            capsize=10,
            color=["#3498db", "#2ecc71"],
            edgecolor="grey",
            alpha=0.8,
        )
        ax1.set_ylabel(f"{name.capitalize()} ({m['unit']})")
        ax1.set_title(f"Mean {name.capitalize()} (score {m['score']}/10)")
        ax1.grid(axis="y", linestyle="--", alpha=0.7)

        ax2.boxplot([m["real"], m["sim"]])
        ax2.set_xticklabels(["Real-World", "ns-3 Sim"])
        ax2.set_ylabel(f"{name.capitalize()} ({m['unit']})")
        ax2.set_title("Distribution")
        ax2.grid(linestyle="--", alpha=0.7)
    if not available:
        axes[0][0].text(
            0.5, 0.5, "No metrics available to plot", ha="center", va="center"
        )
        axes[1][0].axis("off")

    fig.suptitle(
        f"{p_config['name']} Evaluation: Real-World vs ns-3 Sim\n"
        f"Overall Score: {overall_score:.2f}/10"
        if overall_score is not None
        else f"{p_config['name']} Evaluation: Real-World vs ns-3 Sim\nOverall Score: N/A"
    )
    plt.tight_layout()
    plt.savefig(plot_path)
    plt.close()
    print(f"Saved comparison plot to: {plot_path}")

    # 3. Save Markdown Report
    md_path = os.path.join(output_dir, f"eval_report_{protocol}_{timestamp}.md")

    metrics_table = ""
    for name, m in metric_results.items():
        if m["available"]:
            metrics_table += (
                f"| {name} | {m['unit']} | {m['mean_real']:.4f} ± {m['std_real']:.4f} | "
                f"{m['mean_sim']:.4f} ± {m['std_sim']:.4f} | {m['z_score']:.4f} | {m['score']}/10 | {m['coverage']} |\n"
            )
        else:
            metrics_table += f"| {name} | {m['unit']} | N/A | N/A | N/A | N/A | {m['coverage']} |\n"

    overall_str = f"{overall_score:.2f}/10" if overall_score is not None else "N/A"
    min_cov_pct = min_run_coverage * 100

    md_content = f"""# Protocol Evaluation Report: {p_config["name"]}

**Timestamp:** {datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
**Overall Score:** {overall_str}

## Metrics Comparison

A metric is scored only when present in at least {min_cov_pct:.0f}% of
recorded runs on both the real-world and ns-3 sides; otherwise it's
reported N/A rather than guessed.

| Metric | Unit | Real-World (mean ± std) | ns-3 Sim (mean ± std) | Z-Score | Score | Coverage |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
{metrics_table}
"""
    if unavailable:
        md_content += f"\n**Not scored (below run-coverage threshold on at least one side):** {', '.join(unavailable)}\n"

    md_content += """
## Statistical Significance & Scoring

- **Z-Score:** `|mean_sim - mean_real| / max(pooled_std, epsilon_floor)`, where
  `pooled_std = sqrt((std_real^2 + std_sim^2) / 2)` and
  `epsilon_floor = max(1e-9, 0.005 * |mean_real|)` - the floor prevents a
  near-zero real-world std (containers on a localhost network are nearly
  deterministic) from turning a negligible mean difference into a runaway
  z-score.
- **Overall score:** mean of per-metric scores
"""
    for name, m in available.items():
        md_content += (
            f"\n### {name} scoring table\n| Max Z-Score | Score |\n| :--- | :--- |\n"
        )
        for threshold, scr in sorted(m["scoring_table"], key=lambda x: x[0]):
            mark = (
                " (Selected)" if m["z_score"] <= threshold and m["score"] == scr else ""
            )
            md_content += f"| $\\le$ {threshold} | {scr}/10{mark} |\n"

    md_content += f"""
## Visualization

![Comparison Plot]({os.path.basename(plot_path)})
"""
    with open(md_path, "w") as f:
        f.write(md_content)
    print(f"Saved Markdown report to: {md_path}")

    print("\n" + "=" * 50)
    print(f"EVALUATION SUMMARY: {p_config['name']}")
    print("=" * 50)
    for name, m in metric_results.items():
        if m["available"]:
            print(
                f"{name}: real {m['mean_real']:.4f} ± {m['std_real']:.4f} {m['unit']} | "
                f"sim {m['mean_sim']:.4f} ± {m['std_sim']:.4f} {m['unit']} | "
                f"z={m['z_score']:.4f} | score {m['score']}/10"
            )
        else:
            print(f"{name}: N/A (coverage {m['coverage']}, below threshold)")
    print(f"Overall Score: {overall_str}")
    print("=" * 50 + "\n")


def main():
    parser = argparse.ArgumentParser(description="ns-3 Conformance Evaluation Bench")
    parser.add_argument("--config", default="config.toml", help="Path to config.toml")
    parser.add_argument(
        "--protocol",
        required=True,
        help="Protocol to evaluate (must be a key under [protocols] in config.toml)",
    )
    parser.add_argument("--runs", type=int, help="Override number of runs")
    parser.add_argument("--timeout", type=int, help="Override run timeout (seconds)")
    args = parser.parse_args()

    if not os.path.exists(args.config):
        print(f"Error: Configuration file '{args.config}' not found.")
        sys.exit(1)

    with open(args.config, "rb") as f:
        config = tomllib.load(f)

    protocol = args.protocol
    if protocol not in config.get("protocols", {}):
        available = ", ".join(sorted(config.get("protocols", {}).keys()))
        print(f"Error: Unknown protocol '{protocol}'. Available: {available}")
        sys.exit(1)
    p_config = config["protocols"][protocol]

    if p_config.get("parser") not in PARSERS:
        print(
            f"Error: Unknown parser '{p_config.get('parser')}' for protocol '{protocol}'. "
            f"Available: {', '.join(PARSERS.keys())}"
        )
        sys.exit(1)

    num_runs = args.runs or config["global"].get("number_of_runs", 5)
    timeout = args.timeout or config["global"].get("timeout", 60)
    output_dir = config["global"].get("output_dir", "./eval_results")

    client = docker.from_env()

    real_metrics, real_raw = run_real_world(
        client, protocol, config, num_runs, timeout, output_dir
    )
    sim_metrics, sim_raw = run_ns3_simulation(
        client, protocol, config, num_runs, timeout, output_dir
    )

    metric_results, overall_score = score_metrics(
        p_config, real_metrics, sim_metrics, config["global"]
    )
    generate_report(
        protocol,
        p_config,
        metric_results,
        overall_score,
        output_dir,
        config["global"].get("min_run_coverage", 0.8),
    )


if __name__ == "__main__":
    main()
