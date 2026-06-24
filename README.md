# _ns-3 Interoperability_

Interfacing ns-3 simulations with real-world applications.

## Quick Start

1. **Install the Docker network plugin** (required once):

```bash
    sudo ./install_plugin.sh
```

This builds and enables the `netkit-ns-3` driver that connects the
ns-3 simulator to the client container.

2. Start the ns-3 simulator and client:

```bash
       docker compose up -d
```

3. Run the connectivity test (reaches the ns-3 node (see [Network Setup](#network-setup)):

```bash
   docker compose exec client sh examples/ping_ns3.sh
```

4. Override the default command with your own application (see below).

## Network Setup

The compose file creates a single L2 network (`sim_net`) on the `10.10.0.0/24` subnet with static IP assignments:

| Service | Default IP  |
| ------- | ----------- |
| ns3     | `10.10.0.1` |
| client  | `10.10.0.2` |

To change the subnet or IPs, edit the
`networks`>`sim_net`>`ipam` and
`services`>`<name>`>`networks`>`sim_net`>`ipv4_address` fields in `docker-compose.yml`.

> [!IMPORTANT]
> Each client needs its own network.
> To add another client, create an additional network block (also using the `netkit-ns-3` driver with a unique `if-prefix`) and attach it to the new service.

## Running an ns-3 Simulation

Write your simulation script as a `.cc` file inside `./ns3/scratch/` (this is bind-mounted to `/app/ns-3/scratch/scripts` in the container).

You can run it either way:

### Via `docker compose exec` (container stays alive)

```bash
docker compose exec ns3 /app/ns-3/run_sim.sh my-simulation
```

This activates the Python virtual environment, reconfigures ns-3 with the desired build profile, and runs `./ns3 run my-simulation`.
The container keeps running so you can run multiple simulations or use the client.

### Via compose override (auto-run on startup)

Uncomment the `command` line under the `ns3` service in `docker-compose.yml` or use a `docker-compose.override.yml`:

```yaml
services:
  ns3:
    command: ["/app/ns-3/run_sim.sh", "my-simulation"]
```

With this, `docker compose up` will build and run the simulation immediately.
The container exits when the simulation finishes.

The simulation runs inside the ns-3 container and can reach the client container at its default IP (see [Network Setup](#network-setup)) via the simulated network.

## Adding client application

The `client` container mounts the entire `client-node/` directory at `/app`.
Put your code there and override the command in `docker-compose.yml` in `services`>`client`>`command`.

### Examples

```yaml
services:
  client:
    # Run a script from client-node/examples/ (no rebuild needed)
    command: ["sh", "examples/ping_ns3.sh"]

    # Run your own script placed in client-node/
    command: ["python3", "my_app.py", "--flag", "value"]

    # Run a compiled binary
    # Point a binary at the ns-3 node (default IP; see Network Setup section)
    command: ["./my_binary", "--server", "10.10.0.1"]
```

### When to use the `ethtool` fix

Some applications (e.g. those sending raw or unusual packets) need TX checksum offload disabled on the simulated interface.
If you see packet drops or corrupt packets, disable it by uncommenting the line in [`client-node/entrypoint.sh`](./client-node/entrypoint.sh)

```bash
# Uncomment to disable transmit checksum offload
ethtool -K nk0 tx off >/dev/null 2>&1 || true
```

## README for Docker Network Driver Plugin

Check the [Plugin's README.md](./PLUGIN-README.md)!
