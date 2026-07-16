import re
import json

def parse_iperf3(stdout_text):
    """
    Parses iperf3 output (both TCP and UDP).
    Tries to parse as JSON first (if -J flag was used),
    then falls back to regex parser.
    Returns:
        dict: {'throughput': float, 'jitter': float, 'loss': float}
    """
    result = {'throughput': 0.0, 'jitter': 0.0, 'loss': 0.0}
    
    # Try parsing as JSON first
    try:
        data = json.loads(stdout_text)
        if 'end' in data:
            end = data['end']
            # TCP summary
            if 'sum_received' in end:
                # bits per second to Mbps
                bps = end['sum_received'].get('bits_per_second', 0.0)
                result['throughput'] = bps / 1e6
            elif 'sum' in end:
                bps = end['sum'].get('bits_per_second', 0.0)
                result['throughput'] = bps / 1e6
            
            # UDP summary
            if 'sum' in end and 'bits_per_second' in end['sum']:
                bps = end['sum'].get('bits_per_second', 0.0)
                result['throughput'] = bps / 1e6
                result['jitter'] = end['sum'].get('jitter_ms', 0.0)
                result['loss'] = end['sum'].get('lost_percent', 0.0)
            return result
    except (json.JSONDecodeError, TypeError, KeyError):
        pass

    # Fallback to Regex for plain text
    # 1. Throughput (looks for sender/receiver line)
    # E.g.: [  5]   0.00-5.00   sec  5.64 GBytes  9.68 Gbits/sec                  receiver
    # E.g.: [  5]   0.00-5.00   sec  5.64 GBytes  968 Mbits/sec                  receiver
    lines = stdout_text.split('\n')
    receiver_throughput = None
    sender_throughput = None
    
    for line in lines:
        if 'receiver' in line or 'sender' in line:
            # Match number and unit
            match = re.search(r'(\d+(?:\.\d+)?)\s*([GKM]bits/sec)', line)
            if match:
                val = float(match.group(1))
                unit = match.group(2)
                if 'Gbits/sec' in unit:
                    val *= 1000
                elif 'Kbits/sec' in unit:
                    val /= 1000
                if 'receiver' in line:
                    receiver_throughput = val
                else:
                    sender_throughput = val
        
        # Jitter and loss for UDP
        # E.g.: [  5]   0.00-5.00   sec  5.96 MBytes  10.0 Mbits/sec  0.024 ms  0/4249 (0%)
        if 'sec' in line and '/' in line:
            match = re.search(r'(\d+(?:\.\d+)?)\s*ms\s+(\d+)/(\d+)\s*\((\d+(?:\.\d+)?)\%\)', line)
            if match:
                result['jitter'] = float(match.group(1))
                result['loss'] = float(match.group(4))

    if receiver_throughput is not None:
        result['throughput'] = receiver_throughput
    elif sender_throughput is not None:
        result['throughput'] = sender_throughput
        
    return result

def parse_perfdhcp(stdout_text):
    """
    Parses perfdhcp output.
    Returns:
        dict: {'latency': float, 'loss': float}
    """
    result = {'latency': 0.0, 'loss': 0.0}
    
    # Extract average delay (latency)
    # E.g.: avg delay: 1.234 ms
    match_avg = re.search(r'avg delay:\s*(\d+(?:\.\d+)?)\s*ms', stdout_text)
    if match_avg:
        result['latency'] = float(match_avg.group(1))
        
    # Extract drops (loss percentage)
    # E.g. sent packets: 5, received packets: 5, drops: 0
    match_sent = re.search(r'sent packets:\s*(\d+)', stdout_text)
    match_drops = re.search(r'drops:\s*(\d+)', stdout_text)
    
    if match_sent and match_drops:
        sent = int(match_sent.group(1))
        drops = int(match_drops.group(1))
        if sent > 0:
            result['loss'] = (drops / sent) * 100.0
            
    return result

def parse_arping(stdout_text):
    """
    Parses arping output.
    Returns:
        dict: {'latency': float, 'loss': float}
    """
    result = {'latency': 0.0, 'loss': 0.0}
    
    # Try parsing summary line (works for both iputils and habets):
    # E.g. iputils: rtt min/avg/max/mdev = 0.490/0.604/0.812/0.147 ms
    # E.g. habets:  rtt min/avg/max/std-dev = 0.098/0.115/0.150/0.021 ms
    # E.g. habets:  rtt min/avg/max/std-dev = 98.000/115.000/150.000/21.000 usec
    match_summary = re.search(
        r'rtt min/avg/max/(?:mdev|std-dev)\s*=\s*[\d\.]+/([\d\.]+)/[\d\.]+/[\d\.]+\s*([a-zA-Z\xb5\xc2]+)', 
        stdout_text, 
        re.IGNORECASE
    )
    if match_summary:
        val = float(match_summary.group(1))
        unit = match_summary.group(2).lower()
        if 'usec' in unit or 'μs' in unit or '\xb5s' in unit:
            val /= 1000.0
        result['latency'] = val
    else:
        # Fallback: Parse individual lines and compute average
        # E.g. Unicast reply from 10.10.0.1 [00:00:00:00:00:01]  0.812ms
        # E.g. 60 bytes from 00:00:00:00:00:01 (10.10.0.1): index=0 time=115.000 usec
        replies = re.findall(r'(?:reply from.*?|time=)\s*([\d\.]+)\s*(ms|usec|\xb5s|\xc2\xb5s|second|sec)', stdout_text, re.IGNORECASE)
        if replies:
            rtts = []
            for val_str, unit in replies:
                val = float(val_str)
                unit = unit.lower()
                if 'usec' in unit or '\xb5s' in unit or 'μs' in unit:
                    val /= 1000.0
                elif 'second' in unit or unit == 'sec':
                    val *= 1000.0
                rtts.append(val)
            if rtts:
                result['latency'] = sum(rtts) / len(rtts)
            
    # Extract loss
    # E.g. Sent 3 probes (1 broadcast(s)), Received 3 response(s)
    # E.g. 5 packets transmitted, 5 packets received, 0% unanswered
    match_tx = re.search(r'Sent\s*(\d+)\s*probes', stdout_text, re.IGNORECASE)
    match_rx = re.search(r'Received\s*(\d+)\s*response', stdout_text, re.IGNORECASE)
    if match_tx and match_rx:
        tx = int(match_tx.group(1))
        rx = int(match_rx.group(1))
        if tx > 0:
            result['loss'] = ((tx - rx) / tx) * 100.0
    else:
        match_unans = re.search(r'(\d+(?:\.\d+)?)\%\s*unanswered', stdout_text, re.IGNORECASE)
        if match_unans:
            result['loss'] = float(match_unans.group(1))
            
    return result

def parse_ns3_output(stdout_text):
    """
    Parses output from ns-3 simulation scripts.
    We will format our ns-3 scripts to output results in simple parser-friendly lines:
    NS3_METRIC throughput: 94.5 Mbps
    NS3_METRIC latency: 1.25 ms
    NS3_METRIC jitter: 0.12 ms
    NS3_METRIC loss: 0.0 %
    """
    result = {'throughput': 0.0, 'latency': 0.0, 'jitter': 0.0, 'loss': 0.0}
    for line in stdout_text.split('\n'):
        if 'NS3_METRIC' in line:
            match = re.search(r'NS3_METRIC\s+(\w+):\s*(\d+(?:\.\d+)?)\s*([a-zA-Z%]+)?', line)
            if match:
                metric = match.group(1)
                value = float(match.group(2))
                if metric in result:
                    result[metric] = value
    return result
