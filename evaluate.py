#!/usr/bin/env python3
"""
Orchestration script for the Protocol Evaluation Bench.
Spins up real-world containers, runs ns-3 simulations, then scores and
reports (see scoring.py / plotting.py / reporting.py).

Protocol behaviour (packages, commands, cleanup, parser, scored metrics) all
comes from config.toml - adding a protocol needs no changes here.
"""

import argparse
import os
import socket
import subprocess
import sys
import time
import tomllib
from concurrent.futures import ThreadPoolExecutor

import docker

from parsers import PARSERS, parse_ns3_output
from reporting import generate_report
from scoring import score_metrics


def run_pre_install_cmds(container, cmds, label):
    """Runs arbitrary shell commands inside a container before package
    installation."""
    for cmd in cmds:
        print(f"Running pre-install command inside {label} container: {cmd}")
        code, out = container.exec_run(["bash", "-c", cmd], user="root")
        if code != 0:
            print(f"Pre-install command failed ({label}): {out.decode()}")


def provision_container(container, p_config, side):
    """Adds vendor repos and installs packages inside one container.

    Raises RuntimeError on apt-get failure - a missing binary would otherwise
    fail every run and get absorbed as a silent N/A instead of a setup error.
    """
    label = side
    pre_install_cmds = p_config.get(f"{side}_pre_install_cmds", [])
    packages = p_config.get(f"{side}_packages", [])
    if not pre_install_cmds and not packages:
        return  # image already has what it needs - skip apt-get entirely

    run_pre_install_cmds(container, pre_install_cmds, label)
    print(f"Dynamically installing packages inside {label} container...")
    code, out = container.exec_run("apt-get update", user="root")
    if code != 0:
        raise RuntimeError(
            f"{label} apt-get update failed: {out.decode(errors='ignore')}"
        )
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
    """Kills leftover processes from previous runs; transport errors are
    swallowed (cleanup is best-effort, the run loop retries real failures)."""
    for cmd in p_config.get("cleanup_cmds", []):
        try:
            container.exec_run(["sh", "-c", cmd], user="root")
        except Exception as e:
            print(f"  Cleanup warning ({cmd}): {e}")


def get_remote_docker_client(rw_config):
    return docker.DockerClient(
        base_url=f"ssh://{rw_config['remote_ssh_host']}", use_ssh_client=True
    )


def ensure_macvlan_network(docker_client, net_name, parent, subnet):
    try:
        network = docker_client.networks.get(net_name)
        network.remove()
        time.sleep(1)
    except docker.errors.NotFound:
        pass

    return docker_client.networks.create(
        name=net_name,
        driver="macvlan",
        options={"parent": parent},
        ipam=docker.types.IPAMConfig(
            pool_configs=[docker.types.IPAMPool(subnet=subnet)]
        ),
    )


def check_real_world_connectivity(client, config):
    rw_config = config["real_world"]
    ssh_host = rw_config["remote_ssh_host"]
    ok = True

    def report(label, passed, detail=""):
        nonlocal ok
        status = "OK" if passed else "FAIL"
        print(f"  [{status}] {label}" + (f" - {detail}" if detail else ""))
        if not passed:
            ok = False

    print("--- Dry run: checking real-world link setup ---")

    try:
        client.ping()
        report("local Docker daemon reachable", True)
    except Exception as e:
        report("local Docker daemon reachable", False, str(e))

    local_nics = {name for _, name in socket.if_nameindex()}
    report(
        f"local macvlan parent '{rw_config['client_macvlan_parent']}' exists",
        rw_config["client_macvlan_parent"] in local_nics,
    )

    ssh_opts = ["-o", "BatchMode=yes", "-o", "ConnectTimeout=5"]
    ssh_ok = subprocess.run(
        ["ssh", *ssh_opts, ssh_host, "true"], capture_output=True, text=True
    )
    report(
        f"SSH reachable ({ssh_host})",
        ssh_ok.returncode == 0,
        ssh_ok.stderr.strip(),
    )

    nic_result = subprocess.run(
        [
            "ssh",
            *ssh_opts,
            ssh_host,
            "ip",
            "link",
            "show",
            rw_config["server_macvlan_parent"],
        ],
        capture_output=True,
        text=True,
    )
    report(
        f"remote macvlan parent '{rw_config['server_macvlan_parent']}' exists",
        nic_result.returncode == 0,
        nic_result.stderr.strip(),
    )

    try:
        remote_client = get_remote_docker_client(rw_config)
        remote_client.ping()
        report("remote Docker daemon reachable over SSH", True)
    except Exception as e:
        report("remote Docker daemon reachable over SSH", False, str(e))

    print("--- Dry run: " + ("all checks passed" if ok else "checks FAILED") + " ---")
    return ok


def run_real_world(client, remote_client, protocol, config, num_runs, timeout):
    """Runs the real-world baseline: two containers on a physical link,
    repeating the protocol's client command `num_runs` times."""
    print("\n--- Part 1: Running Real-World baseline ---")
    p_config = config["protocols"][protocol]
    rw_config = config["real_world"]
    parser_fn = PARSERS[p_config["parser"]]

    client_net_name = f"eval_client_net_{protocol}"
    server_net_name = f"eval_server_net_{protocol}"

    print(
        f"Creating macvlan network '{client_net_name}' on {rw_config['client_macvlan_parent']}..."
    )
    client_network = ensure_macvlan_network(
        client, client_net_name, rw_config["client_macvlan_parent"], rw_config["subnet"]
    )
    print(
        f"Creating macvlan network '{server_net_name}' on remote {rw_config['server_macvlan_parent']}..."
    )
    server_network = ensure_macvlan_network(
        remote_client,
        server_net_name,
        rw_config["server_macvlan_parent"],
        rw_config["subnet"],
    )

    server_container = None
    client_container = None

    try:
        ensure_image(remote_client, p_config["server_image"])
        ensure_image(client, p_config["client_image"])

        server_name = f"eval_server_{protocol}"
        print(f"Starting server container '{server_name}' on remote host...")
        try:
            c = remote_client.containers.get(server_name)
            c.remove(force=True)
        except docker.errors.NotFound:
            pass

        server_container = remote_client.containers.create(
            image=p_config["server_image"],
            entrypoint=["tail", "-f", "/dev/null"],
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
            entrypoint=["tail", "-f", "/dev/null"],
            name=client_name,
            detach=True,
            cap_add=["NET_ADMIN", "NET_RAW"],
        )
        client_container.start()

        # Provision both sides concurrently (containers still on the bridge
        # network, which is what apt-get's internet access uses).
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

        print("Connecting containers to their macvlan networks...")
        remote_client.networks.get("bridge").disconnect(server_container)
        client.networks.get("bridge").disconnect(client_container)
        server_network.connect(server_container, ipv4_address=rw_config["server_ip"])
        client_network.connect(client_container, ipv4_address=rw_config["client_ip"])

        metrics_per_run = []  # list of dicts, one per run

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
                # server_cmd is often one-shot (iperf3 -s -1) - restart it per
                # attempt or retries hit a dead server.
                if attempt > 1:
                    clean_leftover_processes(server_container, p_config)
                    clean_leftover_processes(client_container, p_config)
                    time.sleep(1)
                print(f"  Starting server: {server_cmd}")
                try:
                    server_container.exec_run(server_cmd, detach=True, user="root")
                except Exception as e:
                    print(f"  Server command failed to start: {e}")
                time.sleep(2)  # Wait for server to bind

                print(
                    f"  Running client (attempt {attempt}/{max_attempts}): {client_cmd}"
                )
                start_time = time.time()
                try:
                    code, out = client_container.exec_run(timed_client_cmd, user="root")
                    stdout_text = out.decode("utf-8", errors="ignore")
                except Exception as e:
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
                # Discard partial output - a truncated run can still match a
                # metric's regex and pollute the mean with a bogus value.
                stdout_text = ""

            res = parser_fn(stdout_text) if code == 0 else {}
            print(f"  Parsed metrics: {res} (Code: {code}, Duration: {duration:.2f}s)")
            metrics_per_run.append(res)

        return metrics_per_run

    finally:
        print("Cleaning up real-world containers and networks...")
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
        if client_network:
            try:
                client_network.remove()
            except Exception:
                pass
        if server_network:
            try:
                server_network.remove()
            except Exception:
                pass


def run_ns3_simulation(client, protocol, config, timeout):
    """Runs the ns-3 simulation baseline. A deterministic sim (the default)
    runs once; one with `deterministic = false` runs `number_of_runs` times
    and is scored on its mean."""
    print("\n--- Part 2: Running ns-3 Simulation baseline ---")
    p_config = config["protocols"][protocol]
    global_cfg = config["global"]
    ns3_script = p_config["ns3_script"]
    deterministic = p_config.get("deterministic", global_cfg.get("deterministic", True))
    num_runs = global_cfg.get("number_of_runs", 5)

    image_tag = "ns3-eval-sim:latest"
    build_prof = global_cfg.get("ns3_build_profile", "optimized")

    try:
        client.images.get(image_tag)
    except docker.errors.ImageNotFound:
        print(f"Image {image_tag} not found. Building it from ns3-node/Dockerfile...")
        client.images.build(
            path="./ns3-node", tag=image_tag, buildargs={"BUILD_PROF": build_prof}
        )

    metrics_per_run = []

    scratch_dir = os.path.abspath("./ns3-node/scratch")
    run_sim_script = os.path.abspath("./ns3-node/run_sim.sh")
    narrow_script = os.path.abspath("./ns3-node/narrow-noarp-interfaces.sh")

    # Physical-link env lives in [global.ns3_env]; protocol ns3_env overrides.
    ns3_env = {str(k): str(v) for k, v in global_cfg.get("ns3_env", {}).items()}
    ns3_env.update({str(k): str(v) for k, v in p_config.get("ns3_env", {}).items()})

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

        # run_sim.sh reconfigures with --enable-examples --enable-tests each
        # call, forcing a full rebuild on a fresh container - floor the
        # timeout so a short one can't SIGKILL a legitimate first-run build.
        ns3_timeout = max(timeout, 300)

        runs = 1 if deterministic else num_runs
        for run in range(1, runs + 1):
            print(f"Run {run}/{runs}...")

            start_time = time.time()
            try:
                # RngRun only matters for stochastic sims, so only pass it
                # when deterministic = false.
                env = {"NS_GLOBAL_VALUE": f"RngRun={run}", **ns3_env}
                if deterministic:
                    env = dict(ns3_env)
                code, out = sim_container.exec_run(
                    [
                        "bash",
                        "-c",
                        f"timeout -k 5 {ns3_timeout}s /app/ns-3/run_sim.sh {ns3_script}",
                    ],
                    user="root",
                    environment=env,
                )
                stdout_text = out.decode("utf-8", errors="ignore")
            except docker.errors.APIError as e:
                print(f"  ns-3 run failed: {e}")
                code, stdout_text = -1, ""
            if code == 124:
                print(f"  ns-3 run timed out after {ns3_timeout}s")
            duration = time.time() - start_time

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

    return metrics_per_run


def main():
    parser = argparse.ArgumentParser(description="ns-3 Accuracy Evaluation Bench")
    parser.add_argument("--config", default="config.toml", help="Path to config.toml")
    parser.add_argument(
        "--protocol",
        help="Protocol to evaluate (must be a key under [protocols] in config.toml)",
    )
    parser.add_argument(
        "--runs",
        type=int,
        help="Override number of real-world runs (defaults to config's number_of_runs; the ns-3 simulation run count follows its deterministic flag)",
    )
    parser.add_argument("--timeout", type=int, help="Override run timeout (seconds)")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify the real-world link setup (Docker/SSH/NICs) and exit, without running an evaluation",
    )
    args = parser.parse_args()

    if not os.path.exists(args.config):
        print(f"Error: Configuration file '{args.config}' not found.")
        sys.exit(1)

    with open(args.config, "rb") as f:
        config = tomllib.load(f)

    if args.check:
        ok = check_real_world_connectivity(docker.from_env(), config)
        sys.exit(0 if ok else 1)

    if not args.protocol:
        print("Error: --protocol is required (unless using --check).")
        sys.exit(1)

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
    deterministic = p_config.get(
        "deterministic", config["global"].get("deterministic", True)
    )

    client = docker.from_env()
    remote_client = get_remote_docker_client(config["real_world"])

    real_metrics = run_real_world(
        client, remote_client, protocol, config, num_runs, timeout
    )
    sim_metrics = run_ns3_simulation(client, protocol, config, timeout)

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
        deterministic,
    )


if __name__ == "__main__":
    main()
