# Protocol Scoring Audit & Deterministic Framework Report

## 1. Executive Summary

This report synthesizes **10 QA/validation reports** spanning **4 test harnesses** (`copilot-cli`, `antigravity-v2`, `claude-code`, `opencode`) and **8 distinct LLM models** (Claude Opus 4.6/4.8, Gemini 3.5 Flash, Deepseek v4 Flash, GPT 5.3 Codex, Kimi K2.6, MiMo v2.5, Minimax M3, Hy3), all evaluating an Internet Control Message Protocol (ICMP) implementation in the ns-3 simulator (`docker-ns3-test`).

### Key Findings

1. **Core architectural delegation.** All reports agree the project writes **zero custom ICMP wire-protocol logic**. It delegates the entire stack to ns-3's stock `Icmpv4L4Protocol` / `Icmpv6L4Protocol` via `InternetStackHelper`; the project C++ (`ping-connection.cc`) is a passive trace-logging scaffold that only observes traffic.

2. **The framing divergence (largest source of variance).** Scores split on *what is being judged*. Evaluators scoring the **delivered end-to-end system** (ns-3 stack included) award **5–7/10**; evaluators scoring the **project's own code contribution** award **2–4/10**. This single framing choice explains ~4 points of variance in RFC-conformance scores. RFC Truthfulness spans 2–7/10; Real-World Utility spans 4–8/10.

3. **Common omissions** flagged across nearly all evaluations:
   - **ICMPv6 (RFC 4443) inactive out-of-the-box** — the simulation script performs no IPv6 address assignment, so ICMPv6 is never exercised even though the ns-3 binary contains a full ND/PTB/MLD implementation.
   - **Narrow message breadth** — only IPv4 Echo Request/Reply (Types 8/0) is actively exercised. No error/control messages (Destination Unreachable, Time Exceeded, Redirect, Parameter Problem) are triggered or verified.

4. **No harness was deterministic.** Frameworks ranged from holistic bands to explicit weighted rubrics (3–7 criteria) to ad-hoc formulas; no two shared a rubric structure, and none used a code-quality/maintainability axis. This report resolves that with a single parameterized formula whose **Delegation Factor** collapses the framing divergence into one tunable term, and generalizes the result to any network protocol.

---

## 2. Normalized Parameter Catalog

All parameters across the 10 reports normalize into **five standard evaluation categories**:

1. **Functional Conformance (FC)** — formatting, field mapping, checksums, address swapping.
2. **Protocol Breadth & Coverage (PB)** — message-type support and dual-stack IPv4/IPv6 completeness.
3. **Edge-Case & Security Handling (ES)** — guard conditions, rate-limiting, input validation.
4. **Simulation Correctness & Fidelity (SF)** — latency realism, topology scaling, impairment modeling.
5. **Real-World Interoperability & Usability (RU)** — tool interop, config ease, debugging hooks.

| Original Parameter | Normalized Category | Source Harness/Model | Description |
|---|---|---|---|
| RFC message coverage (v4+v6) (35%) | Protocol Breadth & Coverage | copilot-cli/gpt-5-3-codex | Presence of IPv4 and IPv6 message processing |
| Protocol-correct packet handling (25%) | Functional Conformance | copilot-cli/gpt-5-3-codex | Standards-based formatting and checksum verification |
| RFC 4443 readiness (IPv6 ICMP) (20%) | Protocol Breadth & Coverage | copilot-cli/gpt-5-3-codex | Active IPv6 logic in the simulation |
| Error/control ICMP behavior breadth (20%) | Protocol Breadth & Coverage | copilot-cli/gpt-5-3-codex | Support for control types beyond echo |
| Message Type Coverage (40%) | Protocol Breadth & Coverage | antigravity-v2/gemini-3-5-flash | Are core error/informational messages active? |
| Correctness of Semantics & Formatting (30%) | Functional Conformance | antigravity-v2/gemini-3-5-flash | Structures, checksums, parsing, state logic |
| Out-of-the-Box Configuration (30%) | Real-World Interop & Usability | antigravity-v2/gemini-3-5-flash | Is the protocol active in the template? |
| Out-of-the-Box Usability (30% of RW) | Real-World Interop & Usability | antigravity-v2/gemini-3-5-flash | Do apps run without manual NIC config? |
| Throughput and Latency Performance (30% of RW) | Simulation Correctness & Fidelity | antigravity-v2/gemini-3-5-flash | User-space context-switch limits under load |
| Protocol Stack Completeness (20% of RW) | Protocol Breadth & Coverage | antigravity-v2/gemini-3-5-flash | Dual-stack IPv4/IPv6 client support |
| Timing and Scheduling Realism (20% of RW) | Simulation Correctness & Fidelity | antigravity-v2/gemini-3-5-flash | Clock-sync drift vs host wall-clock under stress |
| Message Type Coverage (25%) | Protocol Breadth & Coverage | antigravity-v2/opus-4.6 | Fraction of RFC-defined ICMP types supported |
| Header/Checksum Correctness (20%) | Functional Conformance | antigravity-v2/opus-4.6 | Serialized headers and one's-complement |
| Echo Request/Reply Conformance (15%) | Functional Conformance | antigravity-v2/opus-4.6 | Identifier/Sequence/payload echo, src/dst swap |
| Error Message Generation (15%) | Functional Conformance | antigravity-v2/opus-4.6 | Dest Unreach, Time Exceeded structuring |
| Error Suppression Rules (10%) | Edge-Case & Security Handling | antigravity-v2/opus-4.6 | "MUST NOT" rules (no errors to errors/broadcasts) |
| ICMPv6 (RFC 4443) Support (10%) | Protocol Breadth & Coverage | antigravity-v2/opus-4.6 | IPv6 control-logic inclusion and config |
| Rate Limiting & Security (5%) | Edge-Case & Security Handling | antigravity-v2/opus-4.6 | Throttling error generation |
| Basic `ping` compatibility (25% of RW) | Real-World Interop & Usability | antigravity-v2/opus-4.6 | Native OS ping of simulated nodes |
| `traceroute` compatibility (15% of RW) | Real-World Interop & Usability | antigravity-v2/opus-4.6 | Intermediate-node TTL decrement/response |
| Application compatibility (20% of RW) | Real-World Interop & Usability | antigravity-v2/opus-4.6 | Standard TCP/UDP socket apps run |
| Latency/timing fidelity (15% of RW) | Simulation Correctness & Fidelity | antigravity-v2/opus-4.6 | RTT precision vs network distance |
| Topology flexibility (10% of RW) | Simulation Correctness & Fidelity | antigravity-v2/opus-4.6 | Extending beyond a 2-container link |
| Robustness & edge cases (10% of RW) | Edge-Case & Security Handling | antigravity-v2/opus-4.6 | Large packets, fragmentation, PMTUD |
| Documentation & usability (5% of RW) | Real-World Interop & Usability | antigravity-v2/opus-4.6 | Setup guide and troubleshooting |
| Wire-format correctness | Functional Conformance | claude-code/opus-4.8 | Correctness of emitted message bytes |
| Coverage of RFC message types | Protocol Breadth & Coverage | claude-code/opus-4.8 | Breadth of implemented message types |
| Both RFCs (792 and 4443) | Protocol Breadth & Coverage | claude-code/opus-4.8 | Compliance across IPv4 and IPv6 ICMP |
| OS ping interop | Real-World Interop & Usability | claude-code/opus-4.8 | Does a genuine OS ping tool interoperate? |
| Fidelity of conditions | Simulation Correctness & Fidelity | claude-code/opus-4.8 | Realism of simulated network conditions |
| Scope (types, IP versions, scale) | Protocol Breadth & Coverage | claude-code/opus-4.8 | Breadth of types, IP versions, scale |
| Message-type coverage (20%) | Protocol Breadth & Coverage | opencode/hy-3-explicit | Which RFC message types are implemented |
| Header/checksum correctness (20%) | Functional Conformance | opencode/hy-3-explicit | Wire format, pseudo-header, checksum logic |
| Echo request/reply semantics (15%) | Functional Conformance | opencode/hy-3-explicit | id/seq/data copy, src/dst swap, options |
| Error-message structure & guards (20%) | Edge-Case & Security Handling | opencode/hy-3-explicit | Orig IP hdr + 8 bytes; RFC 1122/4443 rules |
| Rate-limiting / robustness (10%) | Edge-Case & Security Handling | opencode/hy-3-explicit | Token bucket, loop prevention |
| Incoming validation (15%) | Edge-Case & Security Handling | opencode/hy-3-explicit | Checksum verification, drop-on-error |
| Checksum correctness (code-level) | Functional Conformance | opencode/kimi-k2.6 | One's-complement + IPv6 pseudo-header |
| Echo/Reply (Types 0/8 & 128/129) | Functional Conformance | opencode/kimi-k2.6 | Field preservation, src/dst swap, payload |
| Error Messages (Type 3, 11) | Functional Conformance | opencode/kimi-k2.6 | IP header + 64 bits of original payload |
| Neighbor Discovery support | Protocol Breadth & Coverage | opencode/kimi-k2.6 | ICMPv6 ND (NS/NA, RS/RA) per RFC 4861 |
| Checksums and Real Hardware | Real-World Interop & Usability | opencode/kimi-k2.6 | Offloaded NIC checksums dropping frames |
| Limitations (Fragmentation, PMTUD) | Edge-Case & Security Handling | opencode/kimi-k2.6 | MTU, fragmentation, specialized queries |
| Implementation completeness | Functional Conformance | opencode/hy-3, minimax-m3 | Custom code logic vs delegation to library |
| PMTU Discovery support | Protocol Breadth & Coverage | opencode/minimax-m3 | Fragmentation Needed / Packet Too Big |
| RFC Truthfulness (Minimax formula) | Functional Conformance | opencode/minimax-m3 | Formula counting types, checksums, fields |
| Real-World Application (Minimax formula) | Real-World Interop & Usability | opencode/minimax-m3 | Formula on tool compatibility/workarounds |
| RFC Truthfulness Rubric (0–10) | Functional Conformance | opencode/deepseek-v4, mimo-v2.5 | Qualitative→numeric feature mapping |
| Real-World Application Rubric (0–10) | Real-World Interop & Usability | opencode/deepseek-v4, mimo-v2.5 | App-interop capability→numeric mapping |
| Checksum offload fragility | Real-World Interop & Usability | antigravity-v2/gemini-3-5-flash | `ethtool` workaround required for real apps |
| Simulation timeout limitation | Simulation Correctness & Fidelity | antigravity-v2/opus-4.6 | Hard 600s simulation stop |

---

## 3. Divergence and Conflict Analysis

### 3.1 Differing Criteria — the "Framing Caveat"

The dominant divergence is whether the auditor evaluates the **underlying ns-3 capability** or the **project's authored code**:

- **System-centric camp** (gemini-3.5-flash, claude-code/opus-4.8, deepseek, kimi-k2.6, copilot-cli): scores the system *as delivered*. Because the node speaks compliant IPv4 ICMP via ns-3, they award **5–8/10**, arguing delegation is a valid design choice and user-facing capability is what matters.
- **Code-centric camp** (opus-4.6, minimax-m3, hy-3 thinking): scores only project-authored code (`ping-connection.cc`, Docker plugin). Since it contains passive logging and zero packet-manipulation logic, they score RFC conformance **2/10**, arguing that calling `InternetStackHelper` is not an ICMP implementation.
- **Hybrid** (hy-3-explicit, 7/10): fetches ns-3.48 source at the tagged version and scores the *library's* internal ICMP implementation directly.

**Consequence:** ~4 points of RFC-truthfulness variance stem from this single framing choice — which is exactly what the Delegation Factor in §4.2 exists to parameterize.

**Rubric granularity differences** (no two evaluators share a structure):

- copilot-cli: 4 criteria (35/25/20/20) · opus-4.6: 7 criteria (25/20/15/15/10/10/5) · hy-3-explicit: 6 criteria (20/20/15/20/10/15) · gemini-3.5-flash: 3 criteria (40/30/30) · minimax-m3: ad-hoc formula · others: holistic bands with no per-criteria weights.

### 3.2 Severity Disagreements

| Issue | Penalty Range | Penalizing heavily | Minimizing |
|---|---|---|---|
| Zero ICMPv6 support | RFC: −1 (opus-4.6) to −4 (mimo-v2.5) | mimo-v2.5, gemini (treats RFC 4443 as ~50% scope) | kimi-k2.6, hy-3-explicit (code present in binary, unconfigured) |
| Missing error message types | RFC: −1.0 (opus-4.6) to −2 (mimo-v2.5) | opus-4.6, deepseek | claude-code (smaller penalty) |
| Only ping tested | RW: −1 (minimax-m3) to −2+ (hy-3 thinking) | hy-3, deepseek | kimi-k2.6 (8/10), minimax-m3 (8/10) |
| Checksum offload (NIC tx-offload) | −2.5 (gemini) to 0 | gemini (critical usability barrier) | kimi-k2.6, minimax-m3 (credit for `ChecksumEnabled=true` + documented `ethtool`) |
| Incoming checksum verification | Only hy-3-explicit flags it | hy-3-explicit (`Icmpv4Header::Deserialize` reads and discards checksum — RFC 792 violation) | all others (ignore; score only outgoing generation) |
| Single-node topology | RW: −1.5 (opus-4.6) to 0 (kimi-k2.6) | opus-4.6, deepseek | kimi-k2.6, minimax-m3 (−0.5) |

**Most contentious:** whether "only ping tested" is a serious limitation or an acceptable baseline. kimi-k2.6/minimax-m3 give 8/10 (ping works); hy-3 thinking gives 4/10 — a **4-point swing on the same factual observation**.

### 3.3 Harness-Specific Nuances

- **antigravity-v2**: longest, most structured reports — explicit sub-score tables and improvement matrices; highest documentation rigor.
- **copilot-cli**: structured ASCII table with clear weight columns — most explicit weighting, briefest analysis.
- **claude-code**: multiplicative model (wire-format × coverage × both-RFCs) rather than additive weights — a fundamentally different math approach.
- **opencode/hy-3 thinking**: uniquely split RFC 792 and RFC 4443 into separate scores (2/10 and 0/10) — the most severe evaluation.
- **opencode/minimax-m3**: only evaluator with an explicit coefficient formula (type coverage, v6 coverage, checksum) — most mathematically explicit, also the most unusual.
- **opencode/hy-3-explicit**: only evaluator to fetch tagged ns-3 source and score the library's ICMP implementation separately from the project.
- **opencode/kimi-k2.6**: highest real-world score (8/10) — focus on successful real-time interop and smart config, most generous on missing features.
- **Topology limit (all harnesses):** the Docker plugin caps each network at exactly 2 containers, forcing single-hop topologies. `Time Exceeded` (TTL expiry) and `Packet Too Big` (PMTUD) paths cannot be verified end-to-end — driving arbitrary "untested" penalties on potentially-correct code.

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

**Key observation:** minimax-m3 has the largest internal spread (6 points) between RFC (2/10, "project contributed nothing") and Real-World (8/10, "but ping works great!") — an unresolved tension in its own rubric about *what* is being evaluated. The Delegation Factor makes this tension an explicit, reportable parameter rather than a hidden inconsistency.

---

## 4. Deterministic Scoring Framework

### 4.1 Qualitative-to-Quantitative Mapping

Every qualitative rubric option across all 10 reports normalizes to fixed values $V \in [0.0, 1.0]$:

| Qualitative Rating | Numerical Value | Definition / Criteria |
|---|---|---|
| **Fully Compliant / Excellent** | `1.00` | All mandatory RFC/operational requirements met; edge cases handled; headers/checksums validated; no drift or workarounds; verified by test evidence. |
| **Adequate / Good** | `0.75` | All core + most optional features present; minor omissions or minor config workaround; core operations stable. |
| **Partial / Fair** | `0.50` | Basic functionality works (e.g., IPv4 Echo/Reply), but major gaps in mandatory features (no active IPv6, missing error types) or significant manual intervention required. |
| **Minimal / Delegated Only** | `0.25` | Behavior exists only transitively via a dependency, or only under highly restricted conditions (passive logging, disabled checks). No project-level logic or active configuration. |
| **Absent / Non-Compliant** | `0.00` | Feature entirely missing, unimplemented, disabled, or failing under all conditions. |

### 4.2 Formula and Weighting

The final deterministic score, of the general form $Score = \sum_i (W_i \times S_i)$, is computed across the five normalized categories with a **Delegation Factor** $D_{FC}$ that resolves the Framing Caveat (§3.1):

$$Score = 10 \times \left( W_{FC} \cdot S_{FC} \cdot D_{FC} + W_{PB} \cdot S_{PB} + W_{ES} \cdot S_{ES} + W_{SF} \cdot S_{SF} + W_{RU} \cdot S_{RU} \right)$$

#### Category Weights ($W_i$, sum = 1.00)

| Category | Weight | Scope |
|---|:---:|---|
| Functional Conformance ($W_{FC}$) | **0.30** | Wire-level packet structure, checksums, address swap, field preservation |
| Protocol Breadth & Coverage ($W_{PB}$) | **0.25** | Message-type completeness, dual-stack IPv4/IPv6 |
| Edge-Case & Security Handling ($W_{ES}$) | **0.15** | Suppression guards, rate-limiting, input validation |
| Simulation Correctness & Fidelity ($W_{SF}$) | **0.15** | Clock sync, topology scale, impairment modeling |
| Real-World Interoperability ($W_{RU}$) | **0.15** | App interop, usability, observability |

#### Delegation Factor ($D_{FC}$) — resolves the Framing Caveat deterministically

- $D_{FC} = 1.0$ — **System Level**: judge whether the delivered product acts as a compliant node (dependency logic counts).
- $D_{FC} = 0.1$ — **Code Level**: judge only the custom code the project authors wrote.

Report *both* scores rather than picking one; the pair is the deterministic replacement for the 4-point framing swing.

#### Category Score Equations ($S_i$, each a weighted sum of normalized sub-parameters $V$)

$$S_{FC} = 0.30\,V_{wire\_format} + 0.30\,V_{checksum\_comp} + 0.20\,V_{address\_swap} + 0.20\,V_{field\_preservation}$$

$$S_{PB} = 0.40\,V_{v4\_msg\_coverage} + 0.40\,V_{v6\_msg\_coverage} + 0.20\,V_{dual\_stack}$$

$$S_{ES} = 0.40\,V_{error\_suppression} + 0.30\,V_{rate\_limiting} + 0.30\,V_{input\_validation}$$

$$S_{SF} = 0.40\,V_{clock\_sync} + 0.30\,V_{topology\_scale} + 0.30\,V_{impairment\_modeling}$$

$$S_{RU} = 0.45\,V_{app\_interop} + 0.35\,V_{usability\_setup} + 0.20\,V_{debug\_observability}$$

Sub-parameter meanings: $V_{wire\_format}$ header layouts · $V_{checksum\_comp}$ checksum incl. pseudo-headers · $V_{address\_swap}$ src/dst swap · $V_{field\_preservation}$ Identifier/Sequence/payload mirroring · $V_{v4/v6\_msg\_coverage}$ fraction of RFC 792 / 4443 types active · $V_{dual\_stack}$ active dual-stack interfaces · $V_{error\_suppression}$ MUST-NOT guards · $V_{rate\_limiting}$ token-bucket throttling · $V_{input\_validation}$ dropping malformed/bad-checksum packets · $V_{clock\_sync}$ schedule resilience under load · $V_{topology\_scale}$ multi-hop support · $V_{impairment\_modeling}$ loss/delay/jitter config · $V_{app\_interop}$ `ping`/`traceroute`/TCP/UDP interop · $V_{usability\_setup}$ absence of manual workarounds (`ethtool`) · $V_{debug\_observability}$ PCAP/trace availability.

#### Interpretation Bands

| Final Score | Rating | Meaning |
|:---:|---|---|
| 9.0–10.0 | **Exemplary** | Full protocol compliance + production-ready interop |
| 7.0–8.9 | **Strong** | Major RFC requirements met; operational for most use cases |
| 5.0–6.9 | **Adequate** | Core works; gaps in breadth, error cases, or dual-stack |
| 3.0–4.9 | **Weak** | Basic echo works; significant RFC omissions; high friction |
| 0.0–2.9 | **Insufficient** | Non-functional, or project contributes no meaningful implementation |

### 4.3 Validation — Scoring `docker-ns3-test`

Applying the formula to the evaluated project:

| Category | Sub-scores | $S_i$ |
|---|---|:---:|
| $S_{FC}$ | wire 1.0, checksum 1.0, swap 1.0, fields 1.0 (ns-3 provides all) | **1.00** |
| $S_{PB}$ | v4 0.5 (Echo only active), v6 0.0 (unconfigured), dual-stack 0.0 | **0.20** |
| $S_{ES}$ | suppression 0.0, rate-limit 0.0, validation 0.5 (checksum discarded on deserialize; length checked) | **0.15** |
| $S_{SF}$ | clock 0.5 (real-time, drifts under load), topology 0.2 (2-container cap), impairment 0.0 | **0.26** |
| $S_{RU}$ | app-interop 0.5 (ping works; traceroute broken single-hop; no TCP/UDP), usability 0.5 (`ethtool` workaround), observability 0.9 (PCAP + trace hooks) | **0.58** |

**Scenario A — System-centric ($D_{FC} = 1.0$):**
$$Score_{sys} = 10 \times (0.30 \cdot 1.0 \cdot 1.0 + 0.25 \cdot 0.20 + 0.15 \cdot 0.15 + 0.15 \cdot 0.26 + 0.15 \cdot 0.58) = 10 \times 0.4985 = \mathbf{5.0/10}$$

**Scenario B — Code-centric ($D_{FC} = 0.1$):**
$$Score_{code} = 10 \times (0.30 \cdot 1.0 \cdot 0.1 + 0.25 \cdot 0.20 + 0.15 \cdot 0.15 + 0.15 \cdot 0.26 + 0.15 \cdot 0.58) = 10 \times 0.2285 = \mathbf{2.3/10}$$

The two scenarios reproduce the exact 5/10-vs-2/10 split observed across the 10 harnesses — now as a single deterministic parameter rather than an unexplained divergence.

---

## 5. Generalized Protocol Scoring Specification

The five normalized categories and the Delegation Factor generalize to any simulated network protocol (transport, routing, application layer).

### 5.1 Generic Parameter Definitions

| Generic Category | Protocol-Specific Mapping | Applies To |
|---|---|---|
| **Conformance — Frame/Packet Format** | Wire-format correctness per standard header spec | TCP, UDP, HTTP/2, QUIC, BGP, OSPF |
| **Conformance — Message Type Coverage** | Which message/frame types / opcodes are implemented | All multi-message protocols |
| **Conformance — Core Operation** | Handshake, session, data-transfer semantics; state transitions | TCP (SYN/SYN-ACK, FSM), HTTP (req/resp), BGP (OPEN/FSM) |
| **Conformance — Error Handling** | Error codes, retransmission, timeout handling | TCP (RST), HTTP (4xx/5xx), BGP (NOTIFICATION) |
| **Conformance — Dual-Stack / IP Version** | IPv4 + IPv6 coverage | TCP, UDP, ICMP, SCTP, DCCP |
| **Robustness — Input Validation** | Checksum/field-range validation, malformed rejection | All wire protocols |
| **Robustness — Rate Limiting / DoS Protection** | Throttling, anti-flood, guards | TCP (SYN flood), ICMP (error rate), HTTP (rate limiting) |
| **Fidelity — Channel Impairment** | Behavior under loss/delay/jitter | Path-dependent protocols |
| **Fidelity — Clock & Event Sync** | Drift under scaling topologies (star/ring/mesh) | Time-dependent protocol behavior |
| **Real-World — Tool Interop** | Compatibility with standard OS tools/client libs | TCP (curl, ssh), HTTP (browsers), DNS (dig), BGP (FRR) |
| **Real-World — Application Breadth** | Multi-protocol / app-level compatibility | All protocols |
| **Real-World — Performance** | Throughput, latency, connection rate under load | Transport & application protocols |
| **Real-World — Operational Friction** | Required workarounds, config complexity, footguns | All emulated/test environments |
| **Real-World — Telemetry & Diagnostics** | PCAP / trace-log accessibility | All protocols |

### 5.2 Checklist for Adapting Weights to Protocol Characteristics

```
Step 1 — Delegation split (always)
──────────────────────────────────
 Report BOTH D_FC = 1.0 (system) and D_FC = 0.1 (code-only). Never collapse silently.

Step 2 — Protocol-type base weights (α = FC+PB conformance share vs SF+RU utility share)
────────────────────────────────────────────────────────────────────────────────────────
 Control/ICMP-layer (ICMP, ARP, ND)  → FC 0.30 PB 0.25 ES 0.15 SF 0.15 RU 0.15   (balanced)
 Transport-layer (TCP, QUIC)         → FC 0.25 PB 0.15 ES 0.25 SF 0.25 RU 0.10   (flow/state/impairment critical)
 Routing-layer (BGP, OSPF, RIP)      → FC 0.25 PB 0.20 ES 0.15 SF 0.30 RU 0.10   (topology scaling/convergence primary)
 Application-layer (HTTP/2, DNS, gRPC) → FC 0.25 PB 0.35 ES 0.10 SF 0.05 RU 0.25   (broad methods/codes; link physics negligible)

Step 3 — Sub-weight nudges by characteristic
─────────────────────────────────────────────
 Connection-oriented (TCP, QUIC)    → raise Core Operation / state-machine sub-weight, lower Message Types.
 Transaction-based (HTTP, DNS)      → raise Error Handling, raise dual-stack (mandatory for DNS/HTTP → 0.15+).
 Stateless (UDP, ICMP)             → lower Core Operation, raise Message Coverage + Error Handling.
 Heavy FSM (BGP, OSPF, TCP)         → add explicit State-Machine Correctness sub-category (~0.15).

Step 4 — Map qualitative rubrics (per §4.1): 0.00 absent · 0.25 delegated/unconfigured · 0.50 core works · 0.75 most met · 1.00 full, test-verified.

Step 5 — Validate: apply to ≥2 independently-written implementations.
         Target ±1 pt cross-evaluator agreement for the same implementation.
         Variance > 2 pts ⇒ a qualitative mapping is ambiguous — tighten it.
```

### 5.3 Protocol-Specific Conformance Templates

**Transport-layer (TCP):**
$$S_{conf}^{TCP} = 0.15Q_{segformat} + 0.15Q_{types} + 0.15Q_{handshake} + 0.15Q_{error} + 0.10Q_{v6} + 0.10Q_{state} + 0.10Q_{validation} + 0.10Q_{congestion}$$

**Routing-layer (BGP):**
$$S_{conf}^{BGP} = 0.10Q_{msgtypes} + 0.10Q_{format} + 0.15Q_{opensession} + 0.15Q_{nlri} + 0.15Q_{fsm} + 0.15Q_{error} + 0.10Q_{policy} + 0.10Q_{timers}$$

**Application-layer (HTTP/1.1):**
$$S_{conf}^{HTTP} = 0.20Q_{methods} + 0.20Q_{headers} + 0.10Q_{statuscodes} + 0.15Q_{persistence} + 0.10Q_{chunking} + 0.10Q_{auth} + 0.15Q_{validation}$$

### 5.4 Pitfalls Identified from This Audit

1. **Delegation ambiguity** — the largest variance driver. Always report the $D_{FC}=1.0$ and $D_{FC}=0.1$ pair rather than picking a camp; expose dependency contribution as an explicit "delegation credit."
2. **Test-evidence requirement** — without verified test output the evaluation is speculative. Require ≥1 verified execution of the core operation before assigning any interop sub-score $> 0$.
3. **Hidden operational friction** — workarounds like `ethtool -K tx off` are scored inconsistently. Cap $V_{usability\_setup} \le 0.50$ for any workaround a user wouldn't discover without reading the README.
4. **Untestable-path penalties** — harness limits (e.g. the 2-container topology cap) block verification of `Time Exceeded` / `Packet Too Big`. Mark these "unverifiable, not absent" rather than penalizing as failures.
5. **Weight convergence** — the §4.2 weights are a *median consensus* across the 10 harnesses, not a theoretical ideal. Retune per protocol via Step 2–3 of the checklist.
