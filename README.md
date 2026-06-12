# NS-3 Netkit Emulation and Postman Integration

This branch sets up L2 peer-to-peer Netkit link between an NS-3 simulator container and a proxy client container to transmit a HTTP message from ns3 to the other container.

## Requirements

1. **Operating System**: Linux with support for Netkit kernel module (Kernel 6.7+ or module loaded).
2. **Docker**: Docker Engine and Docker Compose installed.
3. **Privileges**: Ability to run Docker containers in privileged mode (required by the netkit-init container to manipulate network namespaces and interfaces on the host).
4. **Postman**: Postman Desktop Application installed on the host machine.
5. **Firewall**: UFW or another firewall daemon configured to allow incoming traffic on port 8080.

---

## Configuration and Setup

The network topology consists of a peer-to-peer Netkit link (`nk1` <-> `nk2`) operating in Layer 2 mode:
- **NS-3 Simulator Node**: Binds to `nk1` with simulated IP `10.10.0.1` and hardware MAC `00:00:00:00:00:01`.
- **Proxy Client Node**: Binds to `nk2` with IP `10.10.0.2` and hardware MAC `00:00:00:00:00:02`. Disables TX checksum offloading (`ethtool -K nk2 tx off`) to prevent packet drops inside ns3 TCP/IP stack.
- **Port Forwarding**: The proxy client runs `socat` to forward TCP connections from `10.10.0.2:8080` to host IP `10.10.0.1:8080`.

---

## Executing the Simulation

### 1. Build and Start the Environment
Run the following commands on your host:
```bash
docker compose build
docker compose up -d
```

### (If required) Configure Host Firewall
If host has UFW active, it may block incoming connections from the Docker containers to the host's ports.

To allow the client container's proxy to forward the simulation's HTTP POST request to your host's Postman app.

### 3. Run the Simulation
Execute the NS-3 simulation inside the `ns3-simulator` container:
```bash
docker exec ns3-simulator /usr/src/ns-allinone-3.41/ns-3.41/build/scratch/ns3.41-http-connection-optimized
```

### 4. Output Logs
Upon running, you will see the TCP handshake succeed and the HTTP POST request sent:
```
Simulation main started
ARP Cache is NOT null
Scheduling socket connection...
Starting simulation run...
--- DEBUG INFO at 4.9s ---
Number of interfaces: 2
Interface 0: 127.0.0.1  ARP cache is null pointer = 0
Interface 1: 10.10.0.1  ARP cache is NOT null pointer = 0x560fb3268ae0
-----------------------------------
TCP Connection Succeeded!
Sent HTTP POST packet: hello world
Simulation run ended
Simulation destroyed
```

If you inspect the network traffic using `tcpdump` inside the container:
```bash
docker exec ns3-simulator tcpdump -qns 0 -A -r /usr/src/ns-allinone-3.41/ns-3.41/http-connection-0-0.pcap
```
You will see the complete HTTP POST payload delivered over the Netkit link:
```
POST / HTTP/1.1
Host: 10.10.0.2:8080
Content-Type: text/plain
Content-Length: 11
Connection: close

hello world
```

---

## Troubleshooting

### Mismatched MAC Filtering
If packets are dropped during the TCP handshake:
- Ensure the hardware MAC of the virtual `nk1` interface matches `00:00:00:00:00:01` inside the namespace. The initializer handles this automatically.

### Checksum Offloading Drops
If NS-3 ignores incoming SYN-ACK packets:
- Verify that TX checksum offloading is disabled on the client peer:
  ```bash
  docker exec client ethtool -k nk2 | grep tx-checksum
  ```
  It must show `tx-checksum-ip-generic: off`.
