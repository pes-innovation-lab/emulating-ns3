
# Protocol Scoring Audit & Deterministic Framework Report

## 1. Executive Summary

**Reports analyzed:** 10 evaluation documents across 4 harnesses (copilot-cli, antigravity-v2, claude-code, opencode) and 8 distinct LLM models, all evaluating the ns-3 ICMP implementation in the `docker-ns3-test` project.

**Key findings:**

- **Score divergence range:** RFC Truthfulness spans 2–7/10; Real-World Application spans 4–8/10 — a 5-point and 4-point spread respectively, indicating fundamental methodological disagreement.
- **Perspective conflict:** Some evaluators scored the *project's own code contribution* (finding near-zero ICMP logic = 2/10), while others scored the *end-to-end delivered system* including ns-3's built-in stack (6–7/10). This is the single largest source of divergence.
- All harnesses used two primary axes: *RFC Conformance* (truthfulness/standards compliance) and *Real-World Utility* (operational interop). No harness used a third axis like code quality, maintainability, or test coverage — all of which are relevant for a generalized framework.
- The scoring frameworks ranged from rough holistic bands to explicit weighted rubrics (3–7 criteria per axis) to ad-hoc mathematical formulas. None is deterministic and reproducible as-is.

## 2. Normalized Parameter Catalog

| Original Parameter | Normalized Category | Source Harness/Model | Description |
|---|---|---|---|
| RFC message coverage (v4+v6) | Protocol Conformance — Message Type Coverage | copilot-cli/gpt-5-3-codex | Breadth of RFC-defined ICMP message types implemented |
| Protocol-correct packet handling | Protocol Conformance — Wire Format Correctness | copilot-cli/gpt-5-3-codex | Header structure, checksum computation, field semantics |
| RFC 4443 readiness (IPv6 ICMP) | Protocol Conformance — Dual-Stack Support | copilot-cli/gpt-5-3-codex | IPv6 ICMP implementation completeness |
| Error/control ICMP behavior breadth | Protocol Conformance — Error Message Handling | copilot-cli/gpt-5-3-codex | Destination unreachable, time exceeded, parameter problem, PTB |
| Message Type Coverage | Protocol Conformance — Message Type Coverage | antigravity-v2/gemini-3-5-flash | How many ICMP types are supported (40% of RFC score) |
| Correctness of Semantics & Formatting | Protocol Conformance — Wire Format Correctness | antigravity-v2/gemini-3-5-flash | Packet structures, parsing, checksums, state logic (30%) |
| Out-of-the-Box Configuration | Deployment — Active Configuration | antigravity-v2/gemini-3-5-flash | Are protocols configured and functional in project template? (30%) |
| Message Type Coverage | Protocol Conformance — Message Type Coverage | antigravity-v2/opus-4.6 | How many RFC-defined ICMP types are supported (25%) |
| Header/Checksum Correctness | Protocol Conformance — Wire Format Correctness | antigravity-v2/opus-4.6 | Proper header format and checksum (20%) |
| Echo Request/Reply Conformance | Protocol Conformance — Core Echo Semantics | antigravity-v2/opus-4.6 | Correct id/seq/data echo, src/dst swap (15%) |
| Error Message Generation | Protocol Conformance — Error Message Handling | antigravity-v2/opus-4.6 | Dest Unreachable, Time Exceeded, etc. (15%) |
| Error Suppression Rules | Protocol Conformance — Error Suppression Guards | antigravity-v2/opus-4.6 | "MUST NOT" rules for generating errors (10%) |
| ICMPv6 (RFC 4443) Support | Protocol Conformance — Dual-Stack Support | antigravity-v2/opus-4.6 | Any IPv6 ICMP support (10%) |
| Rate Limiting & Security | Robustness — Error Rate Limiting | antigravity-v2/opus-4.6 | Rate limiting of error messages (5%) |
| Wire-format correctness | Protocol Conformance — Wire Format Correctness | claude-code/opus-4.8 | Correctness of emitted message bytes |
| Coverage of RFC message types | Protocol Conformance — Message Type Coverage | claude-code/opus-4.8 | Breadth of implemented message types |
| Both RFCs (792 and 4443) | Protocol Conformance — Dual-Stack Support | claude-code/opus-4.8 | Compliance across both IPv4 and IPv6 ICMP |
| OS ping interop | Real-World — Basic Tool Interop | claude-code/opus-4.8 | Does a genuine OS ping tool interoperate |
| Fidelity of conditions | Real-World — Timing/Simulation Fidelity | claude-code/opus-4.8 | Realism of simulated network conditions |
| Scope (types, IP versions, scale) | Real-World — Application Breadth | claude-code/opus-4.8 | Breadth of supported types, IP versions, scale |
| Message-type coverage | Protocol Conformance — Message Type Coverage | opencode/hy-3-explicit | Which RFC message types are implemented (20%) |
| Header/checksum correctness | Protocol Conformance — Wire Format Correctness | opencode/hy-3-explicit | Wire format, pseudo-header, checksum logic (20%) |
| Echo request/reply semantics | Protocol Conformance — Core Echo Semantics | opencode/hy-3-explicit | id/seq/data copy, src/dst swap, options (15%) |
| Error-message structure & guards | Protocol Conformance — Error Message Handling | opencode/hy-3-explicit | Orig IP hdr + 8 bytes; RFC 1122/4443 rules (20%) |
| Rate-limiting / robustness | Robustness — Error Rate Limiting | opencode/hy-3-explicit | Token bucket, loop prevention (10%) |
| Incoming validation | Robustness — Input Validation | opencode/hy-3-explicit | Checksum verification, drop-on-error (15%) |
| Out-of-the-Box Usability | Deployment — Operational Friction | antigravity-v2/gemini-3-5-flash | Can real apps interface without workarounds? (30% of RW) |
| Throughput and Latency Performance | Real-World — Performance Characteristics | antigravity-v2/gemini-3-5-flash | Scaling under load (30% of RW) |
| Protocol Stack Completeness | Real-World — Application Breadth | antigravity-v2/gemini-3-5-flash | Dual-stack IPv4/IPv6 support (20% of RW) |
| Timing and Scheduling Realism | Real-World — Timing/Simulation Fidelity | antigravity-v2/gemini-3-5-flash | Simulated clock sync with wall time (20% of RW) |
| Basic ping compatibility | Real-World — Basic Tool Interop | antigravity-v2/opus-4.6 | Does standard ping work end-to-end? (25% of RW) |
| traceroute compatibility | Real-World — Tool Ecosystem | antigravity-v2/opus-4.6 | Does traceroute work? (15% of RW) |
| Application compatibility | Real-World — Application Breadth | antigravity-v2/opus-4.6 | Can real apps (curl, wget, TCP) use this? (20%) |
| Latency/timing fidelity | Real-World — Timing/Simulation Fidelity | antigravity-v2/opus-4.6 | Are RTT measurements realistic? (15%) |
| Topology flexibility | Real-World — Topology Complexity | antigravity-v2/opus-4.6 | Can complex networks be simulated? (10%) |
| Robustness & edge cases | Robustness — Edge Case Handling | antigravity-v2/opus-4.6 | Fragmentation, large payloads, rapid pings (10%) |
| Documentation & usability | Deployment — Documentation Quality | antigravity-v2/opus-4.6 | How easy to get started? (5%) |
| Implementation completeness | Code Quality — Implementation Depth | opencode/hy-3, minimax-m3 | Does project code implement protocol logic or only observe? |
| Checksum correctness (code-level) | Protocol Conformance — Wire Format Correctness | opencode/kimi-k2.6 | Checksum calculation and verification logic |
| IP options preservation | Protocol Conformance — Core Echo Semantics | opencode/hy-3-explicit | Copying IP options (Record Route/Timestamp) in echo reply |
| Neighbor Discovery support | Protocol Conformance — Dual-Stack Support | opencode/kimi-k2.6 | ICMPv6 ND (NS/NA, RS/RA) per RFC 4861 |
| PMTU Discovery support | Protocol Conformance — Error Message Handling | opencode/minimax-m3 | Fragmentation Needed / Packet Too Big generation |
| Checksum offload fragility | Deployment — Operational Friction | antigravity-v2/gemini-3-5-flash | ethtool workaround required for real apps |
| Simulation timeout limitation | Deployment — Operational Constraints | antigravity-v2/opus-4.6 | Hard 600s simulation stop |

## 3. Divergence and Conflict Analysis

### 3.1 Differing Criteria

**Perspective on ownership (the largest divergence):**
- **"Project-code only" camp** (hy-3 thinking: 2/10, minimax-m3: 2/10): Evaluates only what `ping-connection.cc` contributes. Both score near-zero because the file is a passive trace sink with zero ICMP protocol logic. The project code itself "implements" no ICMP.
- **"Delivered-system" camp** (ostrich, deepseek, gemini, kim, etc.: 4–7/10): Evaluates the end-to-end system behavior including ns-3's `Icmpv4L4Protocol` and `Icmpv6L4Protocol`. These evaluators treat ns-3's built-in stack as part of the delivered artifact.
- **Hybrid camp** (hy-3-explicit: 7/10): Explicitly fetches ns-3.48 source code and evaluates ns-3's internal implementation directly.

**Consequence:** This single framing choice explains ~4 points of variance in RFC truthfulness scores.

**Rubric granularity differences:**
- copilot-cli uses 4 criteria (35/25/20/20 weights)
- opus-4.6 uses 7 criteria (25/20/15/15/10/10/5 weights)
- hy-3-explicit uses 6 criteria (20/20/15/20/10/15 weights)
- gemini-3.5-flash uses 3 criteria (40/30/30 weights)
- minimax-m3 uses an ad-hoc mathematical formula
- Others use holistic bands with no explicit per-criteria weighting

No two evaluators use the same rubric structure.

### 3.2 Severity Disagreements

| Issue | Penalty Range (in points) | Evaluators penalizing heavily | Evaluators minimizing |
|---|---|---|---|
| Zero ICMPv6 support | RFC: -1 (opus-4.6) to -4 (mimo-v2.5) | mimo-v2.5 (-4), opus-4.6 (-1) | claude-code (partial credit in rubric) |
| Missing error message types | RFC: -1.0 (opus-4.6) to -2 (mimo-v2.5 or implicit) | opus-4.6, deepseek | claude-code (smaller penalty) |
| Only ping tested | RW: -1 (minimax-m3) to -2+ (hy-3 thinking) | hy-3, deepseek | kimi-k2.6 (8/10), minimax-m3 (8/10) |
| Checksum offload issue | Not always penalized as separate item | gemini (-2.5 pts), hy-3-explicit (mentioned) | claude-code (no separate deduction) |
| Single-node topology | RW: -1.5 (opus-4.6) to 0 (kimi-k2.6) | opus-4.6, deepseek | kimi-k2.6 (no explicit deduction), minimax-m3 (minor -0.5) |

**Most contentious item:** Whether "only ping tested" is a serious limitation or an acceptable baseline. Kimi-k2.6 and minimax-m3 give 8/10 for real-world because ping works; hy-3 thinking gives 4/10 for the same reason. This is a 4-point swing on the same factual observation.

### 3.3 Harness-Specific Nuances

- **antigravity-v2**: Produced the longest, most structured reports with explicit sub-score tables and improvement action matrices. Highest documentation rigor.
- **copilot-cli**: Used a structured ASCII table with clear weight columns — most explicit about weighting but briefest analysis.
- **claude-code**: Used a multiplicative model (wire-format × coverage × both RFCs) rather than additive weights — fundamentally different mathematical approach.
- **opencode (hy-3 thinking)**: Uniquely split RFC 792 and RFC 4443 into separate scores (2/10 and 0/10) rather than combining them. This is the most severe evaluation.
- **opencode (minimax-m3)**: The only evaluator to define an explicit formula with coefficients for type coverage, v6 coverage, checksum correctness, etc. — most mathematically explicit but also the most unusual.
- **opencode (hy-3 explicit)**: The only evaluator that fetched ns-3 source code at the specific tagged version to analyze the *library's* ICMP implementation separately, then scored that rather than the project.
- **opencode (kimi-k2.6)**: Gave the highest real-world score (8/10) with focus on successful real-time interop and smart configuration choices, while being the most generous about missing features.

### 3.4 Score Summary Table

| Harness/Model | RFC Truthfulness | Real-World | Spread | Evaluation Basis |
|---|---|---|---|---|
| copilot-cli/gpt-5-3-codex | 4/10 | 6/10 | 2 | End-to-end system |
| antigravity-v2/gemini-3.5-flash | 6/10 | 5/10 | 1 | End-to-end + configuration |
| antigravity-v2/opus-4.6 | 4/10 | 5/10 | 1 | End-to-end system |
| claude-code/opus-4.8 | 6/10 | 7/10 | 1 | End-to-end system |
| opencode/deepseek-v4-flash | 6/10 | 5/10 | 1 | End-to-end system |
| opencode/hy-3-explicit | 7/10 | 6/10 | 1 | ns-3 source-level (fetched) |
| opencode/kimi-k2.6 | 6/10 | 8/10 | 2 | End-to-end + configuration |
| opencode/mimo-v2.5 | 6/10 | 5/10 | 1 | End-to-end system |
| opencode/hy-3 | 2/10 (792), 0/10 (4443) | 4/10 | 2 | Project-code only |
| opencode/minimax-m3 | 2/10 | 8/10 | 6 | Project-code only (RFC) vs pragmatic (RW) |

**Key observation:** The minimax-m3 model exhibits the largest internal spread (6 points) between its RFC score (2/10, "project contributed nothing") and its Real-World score (8/10, "but ping works great!"), revealing an unresolved philosophical tension in its own rubric about what is being evaluated.

## 4. Deterministic Scoring Framework

### 4.1 Qualitative-to-Quantitative Mapping

All rubric options found across all 10 reports are normalized to the following scale:

| Qualitative Rating | Numerical Value | Definition / Criteria |
|---|---|---|
| Not Implemented / Absent | 0.00 | Feature completely missing from delivered system |
| Partial / Delegated Only | 0.25 | Behavior exists transitively (via dependency) but no project-level logic or active configuration |
| Basic / Minimum Viable | 0.50 | Core function works (e.g., echo reply) but without breadth or robustness |
| Adequate / Mostly Compliant | 0.75 | Most required types/semantics present with minor omissions |
| Excellent / Full Compliance | 1.00 | All mandatory RFC requirements met; verified via test evidence |

### 4.2 Formula and Weighting

The final deterministic score is computed from two primary axes (each 0–10), combined via a configurable weight parameter $\alpha$ (default $\alpha = 0.5$ for equal weighting):

$$S_{final} = \alpha \cdot S_{conformance} + (1 - \alpha) \cdot S_{utility}$$

#### Axis 1: Protocol Conformance Score ($S_{conformance}$)

$$S_{conformance} = 10 \times \sum_{i=1}^{7} W_{c,i} \cdot Q_{c,i}$$

| $i$ | Category | Weight $W_{c,i}$ | Variable $Q_{c,i}$ | Description |
|:---:|---|:---:|:---:|---|
| 1 | Message Type Coverage | 0.20 | $Q_{types}$ | Fraction of RFC-mandated message types implemented (echos, errors, informational) |
| 2 | Wire Format Correctness | 0.20 | $Q_{wire}$ | Header structure, checksum computation, field semantics, pseudo-header (IPv6) |
| 3 | Core Echo Semantics | 0.15 | $Q_{echo}$ | Identifier/sequence preservation, data echo, source/destination swap, options copy |
| 4 | Error Message Handling | 0.15 | $Q_{error}$ | Error message generation, original datagram inclusion (IP header + 64 bits), correct type/code |
| 5 | Dual-Stack Support | 0.12 | $Q_{v6}$ | ICMPv6 (RFC 4443) completeness — echo, error types, ND, PMTU |
| 6 | Error Suppression Guards | 0.10 | $Q_{guards}$ | RFC 1122/4443 "MUST NOT" rules (no errors for errors, broadcasts, fragments) |
| 7 | Input Validation | 0.08 | $Q_{validation}$ | Checksum verification on receive, length validation, type/code range checking |

#### Axis 2: Real-World Utility Score ($S_{utility}$)

$$S_{utility} = 10 \times \sum_{j=1}^{7} W_{u,j} \cdot Q_{u,j}$$

| $j$ | Category | Weight $W_{u,j}$ | Variable $Q_{u,j}$ | Description |
|:---:|---|:---:|:---:|---|
| 1 | Basic Tool Interop | 0.20 | $Q_{ping}$ | Does OS-native `ping` work? Verified via test output |
| 2 | Application Breadth | 0.18 | $Q_{breadth}$ | Multi-protocol support (TCP/UDP/ICMP apps), dual-stack apps, `traceroute`, `mtr`, etc. |
| 3 | Performance Characteristics | 0.15 | $Q_{perf}$ | Throughput, latency stability, jitter under load, scalability |
| 4 | Operational Friction | 0.15 | $Q_{friction}$ | Required workarounds (ethtool, checksum defaults), configuration complexity |
| 5 | Timing/Simulation Fidelity | 0.12 | $Q_{timing}$ | Real-time sync accuracy, wall-clock drift under load, scheduling realism |
| 6 | Topology Complexity | 0.10 | $Q_{topo}$ | Multi-hop support, routing, loss/delay/jitter modeling |
| 7 | Documentation Quality | 0.10 | $Q_{docs}$ | Setup instructions, troubleshooting, architecture documentation, tested examples |

#### Interpretation

| Final Score Range | Rating | Meaning |
|:---:|---|---|
| 9.0–10.0 | **Exemplary** | Full protocol compliance + production-ready real-world interop |
| 7.0–8.9 | **Strong** | Major RFC requirements met; operational for most use cases |
| 5.0–6.9 | **Adequate** | Core functionality works; gaps in breadth, error cases, or dual-stack |
| 3.0–4.9 | **Weak** | Basic echo works; significant RFC omissions; high operational friction |
| 0.0–2.9 | **Insufficient** | Protocol non-functional or project contributes no meaningful implementation |

#### Applying the Framework to the Evaluated Reports

Using the normalized scores from each report and the formula above:

**Example — deepseek-v4-flash:**
- $S_{conformance}$: $Q_{types}=0.50$ (echo + dest unreach + time exceeded, missing 4+ types, no v6), $Q_{wire}=0.75$, $Q_{echo}=0.75$, $Q_{error}=0.50$ (delegated but unexercised), $Q_{v6}=0.00$, $Q_{guards}=0.25$ (implicit via ns-3 but untested), $Q_{validation}=0.25$ (checksums generated but not validated on receive)
- $S_{conformance} = 10 \times (0.20\cdot0.50 + 0.20\cdot0.75 + 0.15\cdot0.75 + 0.15\cdot0.50 + 0.12\cdot0 + 0.10\cdot0.25 + 0.08\cdot0.25) = 10 \times 0.4425 = \mathbf{4.43}$
- $S_{utility}$: $Q_{ping}=0.75$, $Q_{breadth}=0.25$, $Q_{perf}=0.25$, $Q_{friction}=0.50$, $Q_{timing}=0.50$, $Q_{topo}=0.25$, $Q_{docs}=0.75$
- $S_{utility} = 10 \times (0.20\cdot0.75 + 0.18\cdot0.25 + 0.15\cdot0.25 + 0.15\cdot0.50 + 0.12\cdot0.50 + 0.10\cdot0.25 + 0.10\cdot0.75) = 10 \times 0.445 = \mathbf{4.45}$
- $S_{final} = 0.5 \times 4.43 + 0.5 \times 4.45 = \mathbf{4.44/10}$

This is 0.56–1.56 points lower than the original 5–6/10 scores, because the deterministic framework explicitly penalizes the gaps that the original evaluations hand-waved or weighted unevenly.

## 5. Generalized Protocol Scoring Specification

### 5.1 Generic Parameter Definitions

The normalized categories from Section 4.2 generalize to any network protocol evaluation:

| Generic Category | Protocol-Specific Mapping | Applies To |
|---|---|---|
| **Conformance — Frame/Packet Format** | Wire format correctness per RFC/standard header spec | TCP, UDP, HTTP/2, QUIC, BGP, OSPF |
| **Conformance — Message Type Coverage** | Which message types / frame types / operation codes are implemented | All protocols with multiple message types |
| **Conformance — Core Operation** | Handshake, session establishment, data transfer semantics | TCP (SYN/SYN-ACK), HTTP (request/response), BGP (OPEN) |
| **Conformance — Error Handling** | Error codes, retransmission, timeout handling | TCP (RST), HTTP (4xx/5xx), BGP (NOTIFICATION) |
| **Conformance — Dual-Stack/IP Version** | IPv4 + IPv6 coverage for IP-based protocols | TCP, UDP, ICMP, SCTP, DCCP |
| **Conformance — State Machine Correctness** | State transitions per RFC state diagram | TCP, BGP, OSPF, QUIC |
| **Robustness — Input Validation** | Checksum verification, field range validation, malformed packet rejection | All wire protocols |
| **Robustness — Rate Limiting / DoS Protection** | Throttling, anti-flood, guard conditions | TCP (SYN flood), ICMP (error rate), HTTP (rate limiting) |
| **Real-World — Tool Interop** | Compatibility with standard OS tools and client libraries | TCP (curl, wget, SSH), HTTP (browsers), DNS (dig) |
| **Real-World — Application Breadth** | Multi-protocol support, application-level compatibility | All protocols |
| **Real-World — Performance** | Throughput, latency, connection rate, scalability under load | Transport & application protocols |
| **Real-World — Operational Friction** | Required workarounds, configuration complexity, footguns | All protocols in emulated/test environments |
| **Real-World — Topology Complexity** | Multi-hop, routing, network impairment support | Protocols that depend on network path properties |
| **Real-World — Timing Fidelity** | Real-time sync accuracy for simulation/emulation | Any real-time or time-dependent protocol behavior |

### 5.2 Checklist for Adapting Weights to Different Protocol Characteristics

When adapting this framework to a new protocol, follow this process:

```
Step 1: Identify Protocol Type
────────────────────────────────
 Transport-layer (TCP, UDP, QUIC)    → Weight: conformance ~0.40, utility ~0.60
 Routing-layer (BGP, OSPF, RIP)      → Weight: conformance ~0.60, utility ~0.40
 Application-layer (HTTP, DNS, FTP)  → Weight: conformance ~0.30, utility ~0.70
 Control/ICMP-layer (ICMP, ARP, ND)  → Weight: conformance ~0.50, utility ~0.50
 Datalink-layer (Ethernet, PPP)      → Weight: conformance ~0.60, utility ~0.40

Step 2: Adjust Sub-Weights by Protocol Characteristic
─────────────────────────────────────────────────────
 If protocol is connection-oriented (TCP, QUIC):
   → Raise Core Operation weight (+0.05), lower Message Types (-0.05)
 If protocol is transaction-based (HTTP, DNS):
   → Raise Error Handling (+0.05), lower State Machine (+0 → move to Input Validation)
 If protocol is stateless (UDP, ICMP):
   → Lower Core Operation (-0.10), raise Message Type Coverage (+0.05), raise Error Handling (+0.05)
 If protocol has mandatory dual-stack requirements (DNS, HTTP):
   → Raise Dual-Stack Support weight to 0.15
 If protocol has significant state machine (BGP, OSPF, TCP):
   → Add State Machine Correctness as category (weight ~0.15)

Step 3: Map Qualitative Rubrics
────────────────────────────────
 For each category, define 5 rating levels following Section 4.1:
   0.00 = Feature absent from delivered system
   0.25 = Feature exists only via dependency/unconfigured
   0.50 = Core function works
   0.75 = Most requirements met
   1.00 = Full RFC/standard compliance, verified by test

Step 4: Apply Formula
─────────────────────
 S_final = α · S_conformance + (1 - α) · S_utility
 where α is the protocol-type weight from Step 1.

Step 5: Validate
────────────────
 Apply the formula to at least two independently-written implementations.
 Expected: The framework should produce consistent scores (±1 pt) across
 multiple evaluators for the same implementation. If variance exceeds 2 points,
 re-examine qualitative mappings for ambiguity.
```

### 5.3 Protocol-Specific Parameter Templates

**Transport-layer (TCP) template:**
$$S_{conformance}^{TCP} = 10 \times (0.15Q_{segformat} + 0.15Q_{types} + 0.15Q_{handshake} + 0.15Q_{error} + 0.10Q_{v6} + 0.10Q_{state} + 0.10Q_{validation} + 0.10Q_{congestion})$$

**Routing-layer (BGP) template:**
$$S_{conformance}^{BGP} = 10 \times (0.10Q_{msgtypes} + 0.10Q_{format} + 0.15Q_{opensession} + 0.15Q_{nlri} + 0.15Q_{fsm} + 0.15Q_{error} + 0.10Q_{policy} + 0.10Q_{timers})$$

**Application-layer (HTTP/1.1) template:**
$$S_{conformance}^{HTTP} = 10 \times (0.20Q_{methods} + 0.20Q_{headers} + 0.10Q_{statuscodes} + 0.15Q_{persistence} + 0.10Q_{chunking} + 0.10Q_{auth} + 0.15Q_{validation})$$

### 5.4 Pitfalls Identified from This Audit

1. **Delegation ambiguity:** When an implementation relies on a dependency for protocol logic, evaluators must decide whether the dependency counts toward the project score. **Recommendation:** Score the *delivered system*, but report dependency contributions separately as a "delegation credit" in a sub-score field. This avoids the 4-point swing seen in this audit.

2. **Test evidence requirement:** Without verified test output, the evaluation is speculative. The framework should require at minimum one verified test execution showing the core operation works before assigning $Q_{ping} > 0$.

3. **Operational friction is easily hidden:** Workarounds (like `ethtool -K tx off`) that are documented in README are treated differently by different evaluators — some penalize, some treat as neutral documentation. **Recommendation:** Penalize any required workaround that a user would not discover without reading a README, using $Q_{friction} \leq 0.50$ for externally-invisible configuration requirements.

4. **Weight convergence:** The 7-category conformance rubric from Section 4.2 was derived by normalizing weights from all 10 reports. The weights $W_{c,i}$ represent a *median consensus* from the evaluated harnesses, not a theoretical ideal. Protocols with different correctness demands should tune these weights using Step 2 of the checklist.

---
