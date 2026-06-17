# NS-3 Netkit Emulation Bidirectional ARP and ICMP Simulation

This branch demonstrates a peer-to-peer Layer 2 Netkit link established between an NS-3 simulator container and a client container. In this setup, the simulator triggers an ARP Request to the client which responds with an ARP Reply. When the client pings the simulator, both containers trace, parse, and log the resulting ARP and ICMP packets directly to their standard outputs.

## Requirements

1. **Operating System**: Linux with Netkit kernel module support (Kernel 6.7+ or Netkit module loaded).
2. **Docker**: Docker Engine and Docker Compose installed.
3. **Privileges**: Ability to run containers in privileged mode (required by the network initializer helper container to configure host namespaces and interfaces).

---

## Configuration and Setup

The network topology consists of a peer-to-peer Netkit link (`nk1` <-> `nk2`) operating in Layer 2 mode:
- **NS-3 Simulator Node**: Binds to the `nk1` interface with the simulated IP address `10.10.0.1` and hardware MAC `00:00:00:00:00:01`.
- **Client Node**: Binds to the `nk2` interface with the IP address `10.10.0.2` and hardware MAC `00:00:00:00:00:02`.
- **Traffic Tracking**:
  - The simulator hooks into the NetDevice `MacTx` and `MacRx` trace sources to parse and log ARP and ICMPv4 packets.
  - The client container runs a Python raw socket sniffer on `nk2` using `ETH_P_ALL` to log both sent and received ARP and ICMP packets.

---

## Executing the Simulation

### 1. Build and Start the Environment
Run the following command on the host to build and launch the container stack:
```bash
docker compose down && docker compose up -d
```

### 2. Run the Simulation
Execute the NS-3 simulation in the `ns3-simulator` container:
```bash
docker exec ns3-simulator /bin/bash -c "cd /app/ns-3 && ./ns3 run scratch/scripts/arp-connection.cc"
```

### 3. Trigger Bidirectional Exchange
While the simulation is running (within its 15-second simulation window), trigger a ping from the `client` container in a separate terminal:
```bash
docker exec client ping -c 2 10.10.0.1
```

### 4. Verify Output Logs - NS-3 Simulator (Stdout)
The simulator console logs will show the decoded trace messages for both ARP and ICMP packets:
```
Simulation main started
Scheduling ARP trigger packet...
Starting simulation run...
NS-3: Sent ARP Request
NS-3: Received ARP Response
NS-3: Received ICMP Dest Unreachable
NS-3: Received ICMP Echo Request
NS-3: Sent ICMP Echo Reply
NS-3: Received ARP Request
NS-3: Sent ARP Response
NS-3: Received ICMP Echo Request
NS-3: Sent ICMP Echo Reply
Simulation run ended
Simulation destroyed
```

### 5. Verify Client-Side Logs
Check the logs of the `client` container to verify it captured the ARP and ICMP packets:
```bash
docker logs client
```

Output:
```
Client: Received ARP Request - Who has 10.10.0.2? Tell 10.10.0.1
Client: Sent ARP Response - 10.10.0.2 is at 00:00:00:00:00:02
Client: Sent ICMP Dest Unreachable from 10.10.0.2
Client: Sent ICMP Echo Request - 10.10.0.2 -> 10.10.0.1
Client: Received ICMP Echo Reply - 10.10.0.1 -> 10.10.0.2
Client: Sent ARP Request - Who has 10.10.0.1? Tell 10.10.0.2
Client: Received ARP Response - 10.10.0.1 is at 00:00:00:00:00:01
Client: Sent ICMP Echo Request - 10.10.0.2 -> 10.10.0.1
Client: Received ICMP Echo Reply - 10.10.0.1 -> 10.10.0.2
```

### 6. Verify Traffic via tcpdump
To verify the bidirectional ARP and ICMP transaction packet capture, run tcpdump against the generated pcap file:
```bash
docker exec ns3-simulator tcpdump -e -n -r /app/ns-3/arp-connection-0-0.pcap
```
