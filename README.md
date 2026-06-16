# NS-3 Netkit Emulation Bidirectional HTTP Connection

This branch demonstrates a peer-to-peer Layer 2 Netkit link established between an NS-3 simulator container and a client container. In this setup, the simulator sends an HTTP POST request to the client container, which responds with an HTTP response containing a payload. The simulator then reads and prints this response to stdout.

## Requirements

1. **Operating System**: Linux with Netkit kernel module support (Kernel 6.7+ or Netkit module loaded).
2. **Docker**: Docker Engine and Docker Compose installed.
3. **Privileges**: Ability to run containers in privileged mode (required by the network initializer helper container to configure host namespaces and interfaces).

---

## Configuration and Setup

The network topology consists of a peer-to-peer Netkit link (`nk1` <-> `nk2`) operating in Layer 2 mode:
- **NS-3 Simulator Node**: Binds to the `nk1` interface with the simulated IP address `10.10.0.1` and hardware MAC `00:00:00:00:00:01`.
- **Client Node**: Binds to the `nk2` interface with the IP address `10.10.0.2` and hardware MAC `00:00:00:00:00:02`. TX checksum offloading is disabled (`ethtool -K nk2 tx off`) on this interface to prevent the NS-3 TCP/IP stack from rejecting packets.
- **HTTP Server**: The client container runs `socat` listening on port 8080, serving a pre-configured response file (`/etc/newman/response.txt`) with the body `"hello ns3"`.

---

## Executing the Simulation

### 1. Build and Start the Environment
Run the following command on the host to build and launch the container stack:
```bash
chmod +x
docker compose down && docker compose up -d
```

### 2. Run the Simulation
Execute the NS-3 simulation in the `ns3-simulator` container:
```bash
docker exec ns3-simulator ./run_sim.sh http-connection
```

### 3. Verify Output Logs
The simulator logs will show the successful TCP connection, the HTTP POST transmission, and the HTTP response received from the client:
```
Simulation main started
Scheduling socket connection...
Starting simulation run...
TCP Connection Succeeded!
Sent HTTP POST packet: hello world
Received response: HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 9
Connection: close

hello ns3
Simulation run ended
Simulation destroyed
```

### 4. Verify Traffic via tcpdump
To verify the bidirectional HTTP transaction packet capture, run tcpdump against the generated pcap file:
```bash
docker exec ns3-simulator tcpdump -qns 0 -A -r /app/ns-3/http-connection-0-0.pcap
```

The output will display the complete HTTP POST request followed by the HTTP 200 OK response:
```
POST / HTTP/1.1
Host: 10.10.0.2:8080
Content-Type: text/plain
Content-Length: 11
Connection: close

hello world

HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 9
Connection: close

hello ns3
```

---

## Troubleshooting

### Mismatched MAC Filtering
If TCP packets are silently dropped during connection setup:
- Verify that the MAC address of `nk1` inside the simulator's network namespace is explicitly set to `00:00:00:00:00:01`.

### Checksum Offloading Drops
If NS-3 ignores the incoming SYN-ACK packet:
- Ensure TX checksum offloading is disabled on the client side:
  ```bash
  docker exec client ethtool -k nk2 | grep tx-checksum
  ```
  It must return `tx-checksum-ip-generic: off`.
