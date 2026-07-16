import json
import re


def parse_iperf3(stdout_text):
    """
    Parses iperf3 output (both TCP and UDP).
    Tries to parse as JSON first (if -J flag was used),
    then falls back to regex parser.
    Only returns keys that were actually found in the output -
    a metric absent from the output is left out of the dict entirely,
    never defaulted to 0.0 (0.0 can be a real measured value).
    Returns:
        dict: subset of {'throughput': float, 'latency': float, 'jitter': float,
                          'loss': float, 'retransmits': float, 'snd_cwnd': float}
    """
    result = {}

    # Try parsing as JSON first
    try:
        data = json.loads(stdout_text)
        if "end" in data:
            end = data["end"]
            if "sum_received" in end:
                result["throughput"] = (
                    end["sum_received"].get("bits_per_second", 0.0) / 1e6
                )
                # TCP_INFO per-stream stats (Linux kernel only). iperf3's
                # actual field names are mean_rtt/min_rtt/max_rtt (usec) and
                # max_snd_cwnd (bytes), not rtt/rttvar/snd_cwnd - verified
                # against real iperf3 3.16 -J output. There's no rttvar
                # field in this schema at all, so jitter uses max_rtt -
                # min_rtt (the observed RTT spread) as the closest available
                # proxy instead.
                streams = end.get("streams", [])
                if streams and "sender" in streams[0]:
                    sender = streams[0]["sender"]
                    if "mean_rtt" in sender:
                        result["latency"] = sender["mean_rtt"] / 1000.0  # usec -> ms
                    if "max_rtt" in sender and "min_rtt" in sender:
                        result["jitter"] = (sender["max_rtt"] - sender["min_rtt"]) / 1000.0
                    if "retransmits" in sender:
                        result["retransmits"] = float(sender["retransmits"])
                    if "max_snd_cwnd" in sender:
                        result["snd_cwnd"] = sender["max_snd_cwnd"] / 1024.0  # bytes -> KB
            if "sum" in end and "bits_per_second" in end["sum"]:
                result["throughput"] = end["sum"].get("bits_per_second", 0.0) / 1e6
                if "jitter_ms" in end["sum"]:
                    result["jitter"] = end["sum"]["jitter_ms"]
                if "lost_percent" in end["sum"]:
                    result["loss"] = end["sum"]["lost_percent"]
            if result:
                return result
    except (json.JSONDecodeError, TypeError, KeyError):
        pass

    # Fallback to regex for plain text
    lines = stdout_text.split("\n")
    receiver_throughput = None
    sender_throughput = None

    for line in lines:
        if "receiver" in line or "sender" in line:
            match = re.search(r"(\d+(?:\.\d+)?)\s*([GKM]bits/sec)", line)
            if match:
                val = float(match.group(1))
                unit = match.group(2)
                if "Gbits/sec" in unit:
                    val *= 1000
                elif "Kbits/sec" in unit:
                    val /= 1000
                if "receiver" in line:
                    receiver_throughput = val
                else:
                    sender_throughput = val

        # Jitter and loss for UDP
        # E.g.: [  5]   0.00-5.00   sec  5.96 MBytes  10.0 Mbits/sec  0.024 ms  0/4249 (0%)
        if "sec" in line and "/" in line:
            match = re.search(
                r"(\d+(?:\.\d+)?)\s*ms\s+(\d+)/(\d+)\s*\((\d+(?:\.\d+)?)\%\)", line
            )
            if match:
                result["jitter"] = float(match.group(1))
                result["loss"] = float(match.group(4))

    if receiver_throughput is not None:
        result["throughput"] = receiver_throughput
    elif sender_throughput is not None:
        result["throughput"] = sender_throughput

    return result


def parse_perfdhcp(stdout_text):
    """
    Parses perfdhcp output (DISCOVER-OFFER section: sent/received/drops,
    avg delay). Requires client_cmd to pass -r<rate> -p<period> -W<wait_us>
    (see config.toml) - without -W, perfdhcp's report/exit races the actual
    reply arrival and every exchange reads as dropped even though the
    socket-level recvmsg succeeds (confirmed via strace: real OFFER payload
    received, xid/chaddr matching, but still counted as 0 received). -r/-p
    additionally bound the run instead of relying on -n, which was observed
    to be ignored once -W is set.
    Returns:
        dict: subset of {'latency': float, 'loss': float}
    """
    result = {}

    match_avg = re.search(r"avg delay:\s*(\d+(?:\.\d+)?)\s*ms", stdout_text)
    if match_avg:
        result["latency"] = float(match_avg.group(1))

    match_sent = re.search(r"sent packets:\s*(\d+)", stdout_text)
    match_drops = re.search(r"drops:\s*(\d+)", stdout_text)

    if match_sent and match_drops:
        sent = int(match_sent.group(1))
        drops = int(match_drops.group(1))
        if sent > 0:
            result["loss"] = (drops / sent) * 100.0

    return result


def parse_arping(stdout_text):
    """
    Parses arping output.
    Returns:
        dict: subset of {'latency': float, 'jitter': float, 'loss': float}
    """
    result = {}

    # Try parsing summary line (works for both iputils and habets):
    # E.g. iputils: rtt min/avg/max/mdev = 0.490/0.604/0.812/0.147 ms
    # E.g. habets:  rtt min/avg/max/std-dev = 0.098/0.115/0.150/0.021 ms
    # E.g. habets:  rtt min/avg/max/std-dev = 98.000/115.000/150.000/21.000 usec
    match_summary = re.search(
        r"rtt min/avg/max/(?:mdev|std-dev)\s*=\s*"
        r"[\d\.]+/([\d\.]+)/[\d\.]+/([\d\.]+)\s*([a-zA-Z\xb5\xc2]+)",
        stdout_text,
        re.IGNORECASE,
    )
    if match_summary:
        avg, mdev = float(match_summary.group(1)), float(match_summary.group(2))
        unit = match_summary.group(3).lower()
        if "usec" in unit or "μs" in unit or "\xb5s" in unit:
            avg /= 1000.0
            mdev /= 1000.0
        result["latency"] = avg
        # mdev/std-dev is arping's own measured RTT variation across its
        # probes in this run - the direct jitter equivalent.
        result["jitter"] = mdev
    else:
        # Fallback: Parse individual lines and derive mean + stddev
        # E.g. Unicast reply from 10.10.0.1 [00:00:00:00:00:01]  0.812ms
        # E.g. 60 bytes from 00:00:00:00:00:01 (10.10.0.1): index=0 time=115.000 usec
        replies = re.findall(
            r"(?:reply from.*?|time=)\s*([\d\.]+)\s*(ms|usec|\xb5s|\xc2\xb5s|second|sec)",
            stdout_text,
            re.IGNORECASE,
        )
        if replies:
            rtts = []
            for val_str, unit in replies:
                val = float(val_str)
                unit = unit.lower()
                if "usec" in unit or "\xb5s" in unit or "μs" in unit:
                    val /= 1000.0
                elif "second" in unit or unit == "sec":
                    val *= 1000.0
                rtts.append(val)
            if rtts:
                mean = sum(rtts) / len(rtts)
                result["latency"] = mean
                if len(rtts) > 1:
                    variance = sum((r - mean) ** 2 for r in rtts) / len(rtts)
                    result["jitter"] = variance**0.5

    # Extract loss
    # E.g. Sent 3 probes (1 broadcast(s)), Received 3 response(s)
    # E.g. 5 packets transmitted, 5 packets received, 0% unanswered
    match_tx = re.search(r"Sent\s*(\d+)\s*probes", stdout_text, re.IGNORECASE)
    match_rx = re.search(r"Received\s*(\d+)\s*response", stdout_text, re.IGNORECASE)
    if match_tx and match_rx:
        tx = int(match_tx.group(1))
        rx = int(match_rx.group(1))
        if tx > 0:
            result["loss"] = ((tx - rx) / tx) * 100.0
    else:
        match_unans = re.search(
            r"(\d+(?:\.\d+)?)\%\s*unanswered", stdout_text, re.IGNORECASE
        )
        if match_unans:
            result["loss"] = float(match_unans.group(1))

    return result


def parse_ns3_output(stdout_text):
    """
    Parses output from ns-3 simulation scripts.
    ns-3 scripts print parser-friendly lines:
    NS3_METRIC throughput: 94.5 Mbps
    NS3_METRIC latency: 1.25 ms
    NS3_METRIC jitter: 0.12 ms
    NS3_METRIC loss: 0.0 %
    Only metrics actually printed are returned - a metric a given
    sim script doesn't emit is left out, not defaulted to 0.0.
    """
    result = {}
    for line in stdout_text.split("\n"):
        if "NS3_METRIC" in line:
            match = re.search(
                r"NS3_METRIC\s+(\w+):\s*(\d+(?:\.\d+)?)\s*([a-zA-Z%]+)?", line
            )
            if match:
                result[match.group(1)] = float(match.group(2))
    return result


# Registry of real-world traffic-generator parsers, selected by the
# `parser` key in config.toml. The ns-3 side always uses parse_ns3_output
# directly (its output format is fixed by the bench, not user-configurable).
PARSERS = {
    "iperf3": parse_iperf3,
    "perfdhcp": parse_perfdhcp,
    "arping": parse_arping,
}

if __name__ == "__main__":
    # minimal test of parsers
    assert parse_iperf3(
        '{"end": {"sum_received": {"bits_per_second": 94500000.0}}}'
    ) == {"throughput": 94.5}
    assert parse_iperf3("garbage no numbers here") == {}
    assert parse_iperf3(
        '{"end": {"sum_received": {"bits_per_second": 94500000.0}, '
        '"streams": [{"sender": {"mean_rtt": 250, "min_rtt": 205, "max_rtt": 295, '
        '"retransmits": 3, "max_snd_cwnd": 131072}}]}}'
    ) == {
        "throughput": 94.5,
        "latency": 0.25,
        "jitter": 0.09,
        "retransmits": 3.0,
        "snd_cwnd": 128.0,
    }
    assert parse_perfdhcp("avg delay: 1.234 ms\nsent packets: 5, drops: 1") == {
        "latency": 1.234,
        "loss": 20.0,
    }
    assert parse_arping("rtt min/avg/max/mdev = 0.490/0.604/0.812/0.147 ms") == {
        "latency": 0.604,
        "jitter": 0.147,
    }
    fallback = parse_arping(
        "Unicast reply from 10.10.0.1 [00:00:00:00:00:01]  0.500ms\n"
        "Unicast reply from 10.10.0.1 [00:00:00:00:00:01]  0.700ms"
    )
    assert fallback["latency"] == 0.6
    assert abs(fallback["jitter"] - 0.1) < 1e-9
    assert parse_ns3_output("NS3_METRIC throughput: 94.5 Mbps") == {"throughput": 94.5}
    assert parse_ns3_output("no metric here") == {}
    print("parsers.py self-check OK")
