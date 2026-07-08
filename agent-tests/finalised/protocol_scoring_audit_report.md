# Protocol Scoring Audit & Deterministic Framework Report

## 1. Executive Summary
This report presents a systematic synthesis of 10 QA and validation reports evaluating an Internet Control Message Protocol (ICMP) emulation implementation in the ns-3 simulator (`emulating-ns3`). The reviewed reports represent evaluations from various LLMs (including Claude Opus, Gemini 3.5 Flash, Deepseek v4 Flash, GPT 5.3 Codex, Kimi K2.6, and others) operating under different test harnesses (such as `antigravity-v2`, `claude-code`, `copilot-cli`, and `opencode`).

### Key Findings
1. **Core Architectural Delegation**: All reports agree that the target project writes zero custom ICMP wire protocol logic. It delegates the protocol stack entirely to ns-3's stock `Icmpv4L4Protocol` and `Icmpv6L4Protocol` via the `InternetStackHelper`. The project's C++ code (`ping-connection.cc`) acts solely as a trace-logging scaffold that observes packet traffic.
2. **Evaluation Divergence (The Framing Caveat)**: The scoring models diverge fundamentally based on their framing. Reports that score the *entire delivered system* (system-level behavior) award scores in the **5/10 to 7/10** range. Conversely, reports that score *the authors' custom code contribution* (code-level behavior) penalize the implementation heavily, yielding scores in the **2/10 to 4/10** range.
3. **Common Omissions**: All evaluations highlight two critical shortcomings:
   - **ICMPv6 (RFC 4443) Inactivity**: Although the Docker L2 plugin and compose configurations support IPv6 subnets, the ns-3 simulation script (`ping-connection.cc`) lacks IPv6 address assignment or setup, rendering ICMPv6 completely inactive out-of-the-box.
   - **Narrow Message Breadth**: The active simulation only exercises IPv4 Echo Request/Reply (Types 8/0). None of the error or control messages (Destination Unreachable, Time Exceeded, Redirect, Parameter Problem, etc.) are actively triggered or verified.
4. **Generalizability**: The normalized framework developed here can be parameterized for any network protocol by abstracting RFC conformance into standardized behavioral, performance, and environmental metrics.

---

## 2. Normalized Parameter Catalog
The metrics, parameters, and criteria across all 10 reports have been normalized into five standard evaluation categories:
1. **Functional Conformance (FC)**: Formatting, field mapping, checksums, and address swapping.
2. **Protocol Breadth & Coverage (PB)**: Message type support and dual-stack IPv4/IPv6 completeness.
3. **Edge-Case & Security Handling (ES)**: Guard conditions, rate-limiting, and validation.
4. **Simulation Correctness & Fidelity (SF)**: Latency realism, topology scaling, and impairment modeling.
5. **Real-World Interoperability & Usability (RU)**: Real utility interop, config ease, and debugging hooks.

| Original Parameter | Normalized Category | Source Harness/Model | Description |
|---|---|---|---|
| Message Type Coverage (40%) | Protocol Breadth & Coverage | Gemini 3.5 Flash | Evaluates if core error and informational messages are active. |
| Correctness of Semantics & Formatting (30%) | Functional Conformance | Gemini 3.5 Flash | Evaluates structures, checksumming, parsing, and state logic. |
| Out-of-the-Box Configuration (30%) | Real-World Interoperability & Usability | Gemini 3.5 Flash | Verifies if the protocol is active in the template. |
| Out-of-the-Box Usability (30%) | Real-World Interoperability & Usability | Gemini 3.5 Flash | Checks if applications run without manual NIC configuration. |
| Throughput and Latency Performance (30%) | Simulation Correctness & Fidelity | Gemini 3.5 Flash | Measures user-space context-switching limits under load. |
| Protocol Stack Completeness (20%) | Protocol Breadth & Coverage | Gemini 3.5 Flash | Support for dual-stack IPv4/IPv6 client applications. |
| Timing and Scheduling Realism (20%) | Simulation Correctness & Fidelity | Gemini 3.5 Flash | Clock synchronization drift vs host wall-clock time under stress. |
| Message Type Coverage (25%) | Protocol Breadth & Coverage | Claude Opus 4.6 | Fraction of RFC-defined ICMP types supported. |
| Header/Checksum Correctness (20%) | Functional Conformance | Claude Opus 4.6 | Correctness of serialized headers and one's complement. |
| Echo Request/Reply Conformance (15%) | Functional Conformance | Claude Opus 4.6 | Handling of Identifier, Sequence, and payload data echoing. |
| Error Message Generation (15%) | Functional Conformance | Claude Opus 4.6 | Structuring and population of Dest Unreach, Time Exceeded, etc. |
| Error Suppression Rules (10%) | Edge-Case & Security Handling | Claude Opus 4.6 | "MUST NOT" rules (no errors in response to errors/broadcasts). |
| ICMPv6 (RFC 4443) Support (10%) | Protocol Breadth & Coverage | Claude Opus 4.6 | Inclusion and configuration of IPv6-based control logic. |
| Rate Limiting & Security (5%) | Edge-Case & Security Handling | Claude Opus 4.6 | Throttling error generation to avoid storms. |
| Basic `ping` compatibility (25%) | Real-World Interoperability & Usability | Claude Opus 4.6 | Ability of native OS utilities to ping simulated nodes. |
| `traceroute` compatibility (15%) | Real-World Interoperability & Usability | Claude Opus 4.6 | Correct intermediate node TTL decrement and response. |
| Application compatibility (20%) | Real-World Interoperability & Usability | Claude Opus 4.6 | Ability of standard socket applications (TCP/UDP) to run. |
| Latency/timing fidelity (15%) | Simulation Correctness & Fidelity | Claude Opus 4.6 | Precision of simulated RTT compared to network distance. |
| Topology flexibility (10%) | Simulation Correctness & Fidelity | Claude Opus 4.6 | Ease of extending the simulation beyond a 2-container link. |
| Robustness & edge cases (10%) | Edge-Case & Security Handling | Claude Opus 4.6 | Large packets, IP fragmentation, and PMTUD handling. |
| Documentation & usability (5%) | Real-World Interoperability & Usability | Claude Opus 4.6 | Step-by-step setup guide and troubleshooting. |
| RFC message coverage (v4+v6) (35%) | Protocol Breadth & Coverage | GPT 5.3 Codex | Presence of IPv4 and IPv6 message processing. |
| Protocol-correct packet handling (25%) | Functional Conformance | GPT 5.3 Codex | Standard-based formatting and checksum verification. |
| RFC 4443 readiness (IPv6 ICMP) (20%) | Protocol Breadth & Coverage | GPT 5.3 Codex | Evaluation of active IPv6 logic in the simulation. |
| Error/control ICMP behavior breadth (20%) | Protocol Breadth & Coverage | GPT 5.3 Codex | Support for control types beyond simple echo exchanges. |
| RFC Truthfulness Rubric (0-10) | Functional Conformance | Deepseek v4 / MiMo v2.5 | General mapping of qualitative features to numerical scores. |
| Real-World Application Rubric (0-10) | Real-World Interoperability & Usability | Deepseek v4 / MiMo v2.5 | Mapping of application interop capability to numerical scores. |
| Checksum Calculation | Functional Conformance | Kimi K2.6 | One's complement correctness and IPv6 pseudo-header checksum. |
| Echo/Echo Reply (Types 0/8 & 128/129) | Functional Conformance | Kimi K2.6 | Preserving fields, src/dst swapping, and payload integrity. |
| Error Messages (Type 3, 11) | Functional Conformance | Kimi K2.6 | Inclusion of IP header + 64 bits of original payload. |
| Missing Mandatory Features | Protocol Breadth & Coverage | Kimi K2.6 | Gap analysis of missing control/status messages. |
| IPv6 Compliance (RFC 4443) | Protocol Breadth & Coverage | Kimi K2.6 | ND (Neighbor Discovery) and Packet Too Big integration. |
| Successful Real-Time Interoperability | Real-World Interoperability & Usability | Kimi K2.6 | Verification of actual OS kernel-to-simulator ping interop. |
| Checksums and Real Hardware | Real-World Interoperability & Usability | Kimi K2.6 | Impact of offloaded NIC checksums dropping emulated frames. |
| Limitations (Fragmentation, PMTUD) | Edge-Case & Security Handling | Kimi K2.6 | Handling of MTU, fragmentation, and specialized queries. |
| RFC Truthfulness (Minimax Formula) | Functional Conformance | Minimax M3 | Numerical formula counting types, checksums, and fields. |
| Real-World Application (Minimax Formula) | Real-World Interoperability & Usability | Minimax M3 | Numerical formula measuring tool compatibility and workarounds. |
| Message-type coverage (20%) | Protocol Breadth & Coverage | Hy3 (Explicit) | Quantification of active message types. |
| Header/checksum correctness (20%) | Functional Conformance | Hy3 (Explicit) | Byte-level alignment and checksum validation. |
| Echo request/reply semantics (15%) | Functional Conformance | Hy3 (Explicit) | Exact copy verification of Identifier, Sequence, and options. |
| Error-message structure & guards (20%) | Edge-Case & Security Handling | Hy3 (Explicit) | Original payload mapping and loops/broadcast generation guards. |
| Rate-limiting / robustness (10%) | Edge-Case & Security Handling | Hy3 (Explicit) | Token-bucket traffic shaping for error responses. |
| Incoming validation (15%) | Functional Conformance | Hy3 (Explicit) | Verifying incoming checksums and discarding invalid packets. |
| Completeness | Functional Conformance | Hy3 (Thinking) | Custom code contribution vs delegation to library stack. |
| IPv6/ICMPv6 presence | Protocol Breadth & Coverage | Hy3 (Thinking) | Assessment of RFC 4443 implementation. |
| Validation | Functional Conformance | Hy3 (Thinking) | Generation and checking of lengths/checksums. |

---

## 3. Divergence and Conflict Analysis
The evaluations exhibit several critical conflicts in opinions, rubrics, and weightings:

### 3.1 Differing Criteria: The "Framing Caveat"
The most prominent divergence is whether the auditor should evaluate the **underlying ns-3 capability** or the **project's custom codebase**.
- **The System-centric View (Gemini 3.5 Flash, Claude 4.8, Deepseek, Kimi)**: These models evaluate the system as delivered. Since the node *speaks* compliant IPv4 ICMP via ns-3, they award scores of **6/10 to 8/10**. They argue that delegation is a valid design decision and the user-facing capability is what matters.
- **The Code-centric View (Claude 4.6, Minimax, Hy3)**: These models focus strictly on the project-authored code (`ping-connection.cc` and the Docker plugin). Since the code contains only passive logging traces and zero packet-manipulation logic, they score RFC compliance at **2/10**. They argue that calling `InternetStackHelper` does not constitute a custom implementation of ICMP.

### 3.2 Severity Disagreements
- **ICMPv6 (RFC 4443) Omission Penalty**: 
  - *Claude 4.6* and *Gemini 3.5 Flash* treat RFC 4443 as 50% of the target scope. Because IPv6 is disabled in the simulation script, they penalize the score heavily (-1.0 to -2.5 raw points).
  - *Kimi K2.6* and *Hy3 (Explicit)* note that ns-3's underlying `Icmpv6L4Protocol` contains high-quality implementations of Neighbor Discovery, Packet Too Big, and MLD. They do not penalize the score as heavily, citing that the code for these features *is* present in the compiled binary, even if unconfigured in the active topology.
- **Checksum Offloading (NIC Tx Offload)**:
  - *Gemini 3.5 Flash* penalizes this issue heavily (-2.5 points), labeling it as a critical usability barrier because standard Linux client applications will fail out-of-the-box until a manual workaround is applied.
  - *Kimi K2.6* and *Minimax M3* view this as a minor operational detail, giving credit to the authors for setting `ChecksumEnabled = true` and documenting the `ethtool` workaround.
- **Incoming Checksum Verification**:
  - *Hy3 (Explicit)* identifies that ns-3's `Icmpv4Header::Deserialize` reads and discards the checksum rather than actively verifying it, which is technically a direct violation of RFC 792. Other models ignore this completely, focusing only on outgoing checksum generation.

### 3.3 Harness-Specific Nuances
- **Topology Limits**: The Docker plugin restricts each network to exactly 2 containers. This limits the evaluations to single-hop topologies. Consequently, intermediate routing scenarios, such as TTL-expired (which generates `Time Exceeded` messages) and Path MTU Discovery (which generates `Packet Too Big` messages), cannot be verified end-to-end, leading to arbitrary penalties for "untested" but potentially correct paths.

---

## 4. Deterministic Scoring Framework
To resolve the divergences and harmonize the evaluations, we define a unified, mathematical framework.

### 4.1 Qualitative-to-Quantitative Mapping
For any evaluated parameter, qualitative states map to fixed numerical values $V \in [0.0, 1.0]$:

| Qualitative Rating | Numerical Value ($V$) | Definition / Criteria |
|---|---|---|
| **Fully Compliant / Excellent** | `1.0` | Fully implements all RFC/operational requirements. Correctly handles edge cases, validates all headers/checksums, and operates without performance drift or workarounds. |
| **Highly Compliant / Good** | `0.8` | Implements all core features and most optional features. Requires minor configuration workarounds, but core operations are fully functional and stable. |
| **Partially Compliant / Fair** | `0.5` | Implements basic functionality (e.g., IPv4 Echo/Reply works), but has major gaps in mandatory features (e.g., no active IPv6, missing critical error types, or requires significant manual intervention). |
| **Minimally Compliant / Poor** | `0.2` | Only a bare minimum subset of functionality works under highly restricted conditions (e.g., passive logging, or requires disabling security/checks). Core protocol semantics are delegated. |
| **Non-Compliant / Absent** | `0.0` | Feature is entirely missing, unimplemented, disabled, or fails completely under all conditions. |

### 4.2 Formula and Weighting
The final deterministic score is computed as:
$$Score = 10 \times \left( W_{FC} \cdot S_{FC} \cdot D_{FC} + W_{PB} \cdot S_{PB} + W_{ES} \cdot S_{ES} + W_{SF} \cdot S_{SF} + W_{RU} \cdot S_{RU} \right)$$

Where $W_i$ represents category weights, $S_i$ represents category scores, and $D_{FC}$ represents the **Delegation Factor** to resolve the Framing Caveat.

#### 1. Category Weights ($W_i$)
- **Functional Conformance ($W_{FC} = 0.30$)**: Core wire-level packet structures, checksums, and swapping.
- **Protocol Breadth & Coverage ($W_{PB} = 0.25$)**: Completeness of message types and dual-stack IPv4/IPv6 support.
- **Edge-Case & Security Handling ($W_{ES} = 0.15$)**: Throttling, error loop prevention, and input validation.
- **Simulation Correctness & Fidelity ($W_{SF} = 0.15$)**: Latency, scheduling accuracy, and topology modeling.
- **Real-World Interoperability ($W_{RU} = 0.15$)**: App compatibility, configuration usability, and logging.

#### 2. Delegation Factor ($D_{FC}$)
- $D_{FC} = 1.0$ if evaluated at the **System Level** (judging whether the delivered product acts as a compliant node).
- $D_{FC} = 0.1$ if evaluated at the **Code Level** (judging only the custom code written by the project authors).

#### 3. Category Score Equations ($S_i$)
Each category score is calculated as a weighted sum of normalized sub-parameters:

$$S_{FC} = 0.30 \cdot V_{wire\_format} + 0.30 \cdot V_{checksum\_comp} + 0.20 \cdot V_{address\_swap} + 0.20 \cdot V_{field\_preservation}$$
- $V_{wire\_format}$: Compliance of packet header layouts.
- $V_{checksum\_comp}$: Correctness of checksum calculations (including pseudo-headers).
- $V_{address\_swap}$: Correct swapping of source and destination addresses.
- $V_{field\_preservation}$: Mirroring of transaction fields (Identifier, Sequence, and payload).

$$S_{PB} = 0.40 \cdot V_{v4\_msg\_coverage} + 0.40 \cdot V_{v6\_msg\_coverage} + 0.20 \cdot V_{dual\_stack}$$
- $V_{v4\_msg\_coverage}$: Fraction of RFC 792 message types active.
- $V_{v6\_msg\_coverage}$: Fraction of RFC 4443 message types active.
- $V_{dual\_stack}$: Active configuration of dual-stack interfaces.

$$S_{ES} = 0.40 \cdot V_{error\_suppression} + 0.30 \cdot V_{rate\_limiting} + 0.30 \cdot V_{input\_validation}$$
- $V_{error\_suppression}$: Guards preventing generating errors in response to errors, broadcasts, or fragments.
- $V_{rate\_limiting}$: Presence of token bucket or rate limiting on outbound error generation.
- $V_{input\_validation}$: Dropping malformed or corrupt packets (verifying incoming checksums).

$$S_{SF} = 0.40 \cdot V_{clock\_sync} + 0.30 \cdot V_{topology\_scale} + 0.30 \cdot V_{impairment\_modeling}$$
- $V_{clock\_sync}$: Resilience of simulator schedule against CPU load/wall-clock drift.
- $V_{topology\_scale}$: Support for multi-hop network paths.
- $V_{impairment\_modeling}$: Configuration of loss, delay, and jitter models on links.

$$S_{RU} = 0.45 \cdot V_{app\_interop} + 0.35 \cdot V_{usability\_setup} + 0.20 \cdot V_{debug\_observability}$$
- $V_{app\_interop}$: Out-of-the-box interop with Linux tools (`ping`, `traceroute`, TCP/UDP sockets).
- V_{usability\_setup}$: Absence of required manual workarounds (like `ethtool` tx-offload hacks).
- $V_{debug\_observability}$: Output of diagnostics, trace logs, and standard PCAP captures.

---

### 4.3 Validation: Synthesis Scoring of `emulating-ns3`
Applying this deterministic formula to the analyzed project:

#### 1. Category Score Calculations
- **Functional Conformance ($S_{FC}$)**:
  - $V_{wire\_format} = 1.0$ (ns-3 provides correct headers)
  - $V_{checksum\_comp} = 1.0$ (correct one's complement/pseudo-header checksums)
  - $V_{address\_swap} = 1.0$ (properly swaps src/dst)
  - $V_{field\_preservation} = 1.0$ (preserves ID, Sequence, and data)
  - $$S_{FC} = 0.30(1.0) + 0.30(1.0) + 0.20(1.0) + 0.20(1.0) = 1.0$$

- **Protocol Breadth & Coverage ($S_{PB}$)**:
  - $V_{v4\_msg\_coverage} = 0.5$ (only Echo/Reply active; Dest Unreach/Time Exceeded implemented in library but inactive)
  - $V_{v6\_msg\_coverage} = 0.0$ (IPv6 stack unconfigured)
  - $V_{dual\_stack} = 0.0$ (only IPv4 active)
  - $$S_{PB} = 0.40(0.5) + 0.40(0.0) + 0.20(0.0) = 0.20$$

- **Edge-Case & Security Handling ($S_{ES}$)**:
  - $V_{error\_suppression} = 0.0$ (guards not configured or verified)
  - $V_{rate\_limiting} = 0.0$ (throttling inactive)
  - $V_{input\_validation} = 0.5$ (received checksum verification bypassed in deserialize, but length checked)
  - $$S_{ES} = 0.40(0.0) + 0.30(0.0) + 0.30(0.5) = 0.15$$

- **Simulation Correctness & Fidelity ($S_{SF}$)**:
  - $V_{clock\_sync} = 0.5$ (real-time mode active, but drifts under load)
  - $V_{topology\_scale} = 0.2$ (single-hop, 2-container link limit)
  - $V_{impairment\_modeling} = 0.0$ (no packet loss, delay, or jitter configured)
  - $$S_{SF} = 0.40(0.5) + 0.30(0.2) + 0.30(0.0) = 0.26$$

- **Real-World Interoperability ($S_{RU}$)**:
  - $V_{app\_interop} = 0.5$ (`ping` works; `traceroute` broken due to single-hop; no TCP/UDP validation)
  - $V_{usability\_setup} = 0.5$ (requires manual `ethtool` checksum offload workaround)
  - $V_{debug\_observability} = 0.9$ (PCAPs generated, trace logging hooks installed)
  - $$S_{RU} = 0.45(0.5) + 0.35(0.5) + 0.20(0.9) = 0.58$$

#### 2. Synthesized Scores
- **Scenario A: System-centric Score ($D_{FC} = 1.0$)**:
  $$Score_{sys} = 10 \times \left( 0.30(1.0)(1.0) + 0.25(0.20) + 0.15(0.15) + 0.15(0.26) + 0.15(0.58) \right)$$
  $$Score_{sys} = 10 \times \left( 0.30 + 0.05 + 0.0225 + 0.039 + 0.087 \right) = 10 \times 0.4985 = \mathbf{5.0 / 10}$$

- **Scenario B: Code-centric Score ($D_{FC} = 0.1$)**:
  $$Score_{code} = 10 \times \left( 0.30(1.0)(0.1) + 0.25(0.20) + 0.15(0.15) + 0.15(0.26) + 0.15(0.58) \right)$$
  $$Score_{code} = 10 \times \left( 0.03 + 0.05 + 0.0225 + 0.039 + 0.087 \right) = 10 \times 0.2285 = \mathbf{2.3 / 10}$$

---

## 5. Generalized Protocol Scoring Specification
To scale this framework to other simulated network protocols (such as transport, routing, or application layer), the parameter set must be translated into generic terms.

### 5.1 Generic Parameter Definitions
- **Protocol Conformance (replacing RFC Truthfulness)**:
  - *Syntactic Correctness*: Validates packet/message layouts, headers, and encoding (e.g., TCP flags, HTTP headers, DNS query structures).
  - *Semantic Correctness*: Correct state transition mappings (e.g., TCP 3-way handshake states, BGP peer state machine).
- **Performance Correctness**:
  - *Congestion & Flow Control*: Correct execution of traffic control algorithms (e.g., TCP BBR/NewReno, HTTP/2 window updates).
  - *Resource Management*: Handling of retries, timeouts, and resource pools (e.g., connection pools, packet buffers).
- **Edge-Case & Security Handling**:
  - *Input Sanitization*: Rejection or safe handling of malformed messages (e.g., HTTP request smuggling, DNS amplification guards).
  - *Rate Throttling*: Traffic-limiting mechanisms under high load or denial-of-service conditions.
- **Simulation Fidelity**:
  - *Channel Impairment Handling*: Performance verification under varying loss, delay, and jitter.
  - *Clock & Event Synchronization*: Drift metrics under scaling topologies (star, ring, mesh configurations).
- **Usability & Application Interoperability**:
  - *Client Integration*: Successful interop with standard OS utilities (e.g., curl, browser clients, BGP daemons).
  - *Telemetry & Diagnostics*: Accessibility of standard packet captures (PCAP) and trace logs.

### 5.2 Weight Adaptation Checklist
Auditors should adjust the category weights ($W_i$) based on the specific protocol type:

- [ ] **Transport-Layer Protocols (e.g., TCP, QUIC)**
  - *Action*: Increase **Edge-Case & Security Handling** to 25% (flow control, retransmissions, state-machine validation) and **Simulation Fidelity** to 25% (link impairments are critical).
  - *Action*: Decrease **Protocol Breadth** to 15% (fewer message types exist compared to application layers).

- [ ] **Routing Protocols (e.g., OSPF, BGP)**
  - *Action*: Increase **Simulation Fidelity** to 30% (topology scaling, path convergence times, multi-node loops are primary).
  - *Action*: Decrease **Real-World Interoperability** to 10% (interoperability is usually tested against a single daemon like FRRouting).

- [ ] **Application-Layer Protocols (e.g., HTTP/2, DNS, gRPC)**
  - *Action*: Increase **Protocol Breadth & Coverage** to 35% (broad range of methods, response codes, headers, and query types).
  - *Action*: Increase **Real-World Interoperability** to 25% (must interoperate seamlessly with a huge variety of browsers, tools, and OS socket layers).
  - *Action*: Decrease **Simulation Fidelity** to 5% (link-level physics like propagation delay have negligible impact on application conformance).
