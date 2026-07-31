We create an evaluation bench to measure how far a given protocol's ns-3
implementation sits from the real-world implementation, scoring it out of 10.
The bench has 2 parts:-

### First part:
2 containers where 2 applications communicate with each other, and log metrics, according to the preferred testing applications for each protocol:-
- iperf3 for TCP, UDP
- perfdhcp for DHCP
- arping for ARP
- ping for Ping

The two containers sit on two physically separate machines, joined by a real
Ethernet cable, not a same-host veth pair - see
[Real-World Link Setup](#real-world-link-setup) for how to wire this up.

### Second Part:

2 nodes inside ns-3 that simulate the exact same exchanges by the 2 applications inside the simulation. The same metrics as the first part are logged.

### Scoring:

The real-world side is characterized as a distribution N(μ_real, σ_real) from
its repeated runs. The simulation side is either a point mass at its single
output (`deterministic = true`) or its own N(μ_sim, σ_sim) (`deterministic =
false`). Both are scored with one formula - the standardized distance of the
sim's center from the real center, in real-world spread units (Glass's Δ),
with a configurable base tolerance (ε) that keeps the denominator well-defined
when the real side measures deterministic or near-deterministic:

```
ε = max(tolerance_abs, tolerance_ratio · |μ_real|)
x = |μ_sim − μ_real| / max(σ_real, ε)
```

x is read against a configurable scoring table (default
`[0.5, 10], [1.0, 9], [1.5, 8], [2.0, 7], [3.0, 5], [5.0, 3], [10.0, 1]`) to
produce the score out of 10. When σ_real = 0 the denominator falls back to ε,
so the score is graded ("within real-world resolution") rather than a binary
pass/fail. The scoring table, tolerance, run counts, and the deterministic
flag are all user-modifiable in `config.toml`.

## Format:

A `config.toml` file that contains:-
- protocol
- scoring table for the standardized distance x
- tolerance (base resolution) and deterministic flag
- container (path/oci)
- timeout
- number of runs
- log_file_location
- corresponding ns-3 script to run

A python script that:-
- reads the config.toml
- spins up the applications
- runs ns-3 simulation
- extracts performance metrics and establish baseline
- compares and scores
- gives output and diff.

## Real-World Link Setup

Two machines, joined by a direct Ethernet cable or a dumb L2 switch (never
a routed hop - ARP/DHCP are link-layer). This machine runs `client`; the
other runs `server` via its own Docker daemon over SSH. Both containers
ride their host's physical NIC via `macvlan`.

**1. NICs**: on each machine, note the cable-connected NIC's name (`ip
link`). It must carry no IP of its own - `macvlan` owns it:

```bash
sudo ip addr flush dev eth1
sudo ip link set eth1 up
```

**2. `config.toml`**:

```toml
[real_world]
client_macvlan_parent = "eth1"        # this machine's NIC
server_macvlan_parent = "eth0"        # remote machine's NIC
subnet = "10.10.0.0/24"
client_ip = "10.10.0.2"
server_ip = "10.10.0.1"
remote_ssh_host = "user@remote-host"
```

**3. Remote Docker over SSH**: `evaluate.py` shells out to the system
`ssh` binary (via docker-py's `use_ssh_client=True`), so it inherits
whatever `~/.ssh/config`/agent setup already makes `ssh <remote_ssh_host>`
work in your terminal. Needs: Docker running on the remote machine,
passwordless auth already working, SSH user in the `docker` group (or root).

**4. Verify**: `python3 evaluate.py --check` - local/remote Docker
reachable, both NICs exist, SSH reachable. Fix failures here before a
real run, or a bad NIC/host name surfaces only after `apt-get update`.

**Calibration**: `sim-*.cc` channel delay constants are calibrated
against a veth pair, not a real cable - re-measure RTT
(`ping`/`arping` between the macvlan IPs) and recalibrate. Doesn't fix
the [Known Modeling Gaps](#known-modeling-gaps) below - orthogonal.

## Known Modeling Gaps

Found while debugging the eval bench's scores against real 10-run evaluations.
Not blockers, but each limits how far a score can be trusted at face value.

- **TCP RTT/latency is queue-dominated in sim, not propagation-dominated.**
  `sim-tcp.cc`'s BulkSend flow saturates the 1Gbps link almost immediately,
  so measured RTT reflects self-induced queuing delay, not the configured
  channel delay. Recalibrating channel delay fixed ARP/DHCP but has no
  effect on TCP's RTT/jitter/throughput scores - those need a different
  fix (non-saturating traffic pattern, or a link capacity closer to what
  the real veth pair can actually sustain).

- **DHCP latency models link delay + a hand-tuned `Collect` wait (100us),
  not real server processing time.** Real-world latency is dominated by
  dnsmasq's own processing (lease allocation, DB write, etc.), which isn't
  modeled at all. The 100us figure was picked to roughly match observed
  real latency, not derived from any protocol/timing model - if the real
  server's processing time shifts, this constant goes stale silently.

- **Channel delay range (2-8us) is calibrated to this specific host's veth
  pair**, from directly measured real ARP/TCP RTT on this machine. It's
  not derived from config or auto-calibrated - on a different host/kernel
  the real veth latency could differ, and scores would quietly drift
  without an obvious signal pointing at the channel delay as the cause.

- **The sims are deterministic by design** (`deterministic = true` in
  config.toml): with no injected error models, a single run is the whole
  answer. Any metric whose real side is also deterministic (σ_real = 0)
  is scored against the tolerance ε rather than a measured spread - a sim
  that misses a deterministic real value by more than the tolerance scores
  zero. This is the deliberate "pure comparison" stance: no artificial
  loss/jitter is injected to manufacture variance.

- **ns-3's TCP congestion control algorithm isn't explicitly matched to
  the real host's.** A captured real iperf3 JSON reported
  `sender_tcp_congestion: "cubic"`; sim-tcp.cc uses whatever ns-3's
  default congestion algorithm is for the build, unverified against
  `cubic`. A mismatch here would affect throughput/cwnd/retransmit
  dynamics independent of any topology/delay tuning.

- **The "real world" baseline is itself a single local veth pair, not a
  representative network.** No real congestion, cross-traffic, or
  WAN-style queuing/loss ever occurs on the real side, so even a sim that
  scores well here isn't validated against conditions a real deployment
  would actually see - the whole bench's conclusions are scoped to "does
  this simulation match an idle local link," not "does it match a real
  network."

- **iperf3 TCP_INFO field names are version-dependent and unverified
  beyond this session's captured version (3.16).** `mean_rtt`/`min_rtt`/
  `max_rtt`/`max_snd_cwnd` were confirmed against one real capture; an
  older or newer iperf3 on a different real-world container image could
  use different field names again, silently reintroducing permanent N/A
  for `latency`/`jitter`/`snd_cwnd` the way the original `rtt`/`rttvar`/
  `snd_cwnd` guess did.
