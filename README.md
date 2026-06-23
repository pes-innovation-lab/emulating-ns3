# NS-3 Netkit Emulation SCTP Simulation

This branch demonstrates SCTP transport-layer communication established between an NS-3 simulator container (acting as an SCTP Server) and a client container (acting as an SCTP Client) over a peer-to-peer Layer 2 Netkit link (`nk1` <-> `nk2` on subnet `10.10.0.0/24`).

The NS-3 container compiles and loads a custom `sctp` L4 protocol module, and the client container utilizes standard Linux kernel SCTP sockets via Python.

## Requirements

1. **Operating System**: Linux with Netkit and SCTP kernel modules loaded (run `sudo modprobe netkit sctp` if not loaded).
2. **Docker**: Docker Engine and Docker Compose installed.
3. **Privileges**: Ability to run containers in privileged mode (required by the network initializer helper container to configure host namespaces and interfaces).

---

## Configuration and Setup

The network topology consists of a peer-to-peer Netkit link (`nk1` <-> `nk2`) operating in Layer 2 mode:
- **NS-3 Simulator Node (SCTP Server)**: Binds to the `nk1` interface with the simulated IP address `10.10.0.1/24` and hardware MAC `00:00:00:00:00:01`. It runs a simulated SCTP Server listening on port `5001`.
- **Client Node (SCTP Client)**: Binds to the `nk2` interface with the statically configured IP address `10.10.0.2/24` and hardware MAC `00:00:00:00:00:02`. It executes a Python SCTP client to exchange data with the simulator.
- **Traffic Tracking**:
  - The simulator hooks into the NetDevice `MacTx` and `MacRx` trace sources to parse and log SCTP and ARP packets directly to stdout.

---

## Executing the Simulation

### 1. Build and Start the Environment
Run the following command on the host to rebuild and launch the container stack:
```bash
docker compose down && docker compose up -d --build
```

### 2. Establish Network Namespace Link
Establish the netkit link and namespaces mapping:
```bash
docker compose restart netkit-init
```

### 3. Run the Simulation
Execute the NS-3 SCTP simulation script in the `ns3-simulator` container:
```bash
docker exec ns3-simulator /bin/bash -c "cd /app/ns-3 && . /app/pyenv/bin/activate && ./ns3 run scratch/scripts/sctp-connection"
```

---

## Verification

### 1. Verify Simulation Output (Stdout)
During the simulation run, the simulator console logs will show the SCTP packets being exchanged:
```
Simulation main started
SCTP Server listening on 10.10.0.1:5001...
Starting simulation run...
NS-3: Received ARP Request
NS-3: Sent ARP Response
NS-3: Received SCTP Packet
NS-3: Sent SCTP Packet
NS-3 Server Received Message: "Hello from Client via SCTP"
NS-3 Server Sent Response
NS-3: Received SCTP Packet
NS-3: Sent SCTP Packet
Simulation run ended
Simulation destroyed
```

### 2. Verify Client-Side Logs
Check the logs of the `client` container to verify that it connected, completed the SCTP exchange, and received the response:
```bash
docker compose logs client
```
Output:
```
SCTP Client starting...
Successfully bound test socket to 10.10.0.2. Interface is ready!
Connected to NS-3 SCTP Server successfully!
Client: Sent SCTP payload "Hello from Client via SCTP"
Client: Received SCTP payload: "Hello from NS-3 Server via SCTP"
Client socket closed.
Client keeping container alive. Press Ctrl+C to stop.
```

### 3. Verify Traffic via tcpdump
To verify the SCTP exchange, run tcpdump against the generated pcap file:
```bash
docker exec ns3-simulator tcpdump -e -n -vv -r /app/ns-3/sctp-connection-0-0.pcap
```
