import socket
import struct

CLIENT_MAC = "00:00:00:00:00:02"

def main():
    # Bind to raw socket for all protocols (0x0003)
    ETH_P_ALL = 0x0003
    # socket.htons(ETH_P_ALL) is 0x0300
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ALL))
    
    # Bind to the nk2 interface with a retry loop to wait for interface creation
    import time
    while True:
        try:
            s.bind(("nk2", ETH_P_ALL))
            print("Successfully bound to nk2.")
            break
        except Exception as e:
            print(f"Waiting for interface nk2 to be ready... ({e})")
            time.sleep(1)

    print("Client listening for ARP and ICMP packets on nk2...")
    while True:
        try:
            packet, addr = s.recvfrom(2048)
            # Parse Ethernet header (first 14 bytes)
            if len(packet) < 14:
                continue
            eth_header = packet[:14]
            dest_mac, src_mac, eth_type = struct.unpack("!6s6sH", eth_header)
            
            src_mac_str = ":".join(f"{b:02x}" for b in src_mac)
            dest_mac_str = ":".join(f"{b:02x}" for b in dest_mac)
            
            is_sent = (src_mac_str.lower() == CLIENT_MAC.lower())
            direction = "Sent" if is_sent else "Received"
            
            if eth_type == 0x0806: # ARP
                arp_header = packet[14:42]
                if len(arp_header) < 28:
                    continue
                hw_type, proto_type, hw_size, proto_size, op = struct.unpack("!HHBBH", arp_header[:8])
                src_ip = socket.inet_ntoa(arp_header[14:18])
                dest_ip = socket.inet_ntoa(arp_header[24:28])
                
                if op == 1:
                    print(f"Client: {direction} ARP Request - Who has {dest_ip}? Tell {src_ip}")
                elif op == 2:
                    print(f"Client: {direction} ARP Response - {src_ip} is at {src_mac_str}")
                    
            elif eth_type == 0x0800: # IPv4
                ip_header = packet[14:34]
                if len(ip_header) < 20:
                    continue
                version_ihl, tos, total_length, ip_id, flags_fragment, ttl, proto, checksum, src_ip_bytes, dest_ip_bytes = struct.unpack("!BBHHHBBH4s4s", ip_header)
                
                if proto == 1: # ICMP
                    ihl = version_ihl & 0x0F
                    ip_header_len = ihl * 4
                    icmp_start = 14 + ip_header_len
                    icmp_header = packet[icmp_start:icmp_start+4]
                    if len(icmp_header) < 4:
                        continue
                    icmp_type, icmp_code, icmp_checksum = struct.unpack("!BBH", icmp_header)
                    
                    src_ip = socket.inet_ntoa(src_ip_bytes)
                    dest_ip = socket.inet_ntoa(dest_ip_bytes)
                    
                    if icmp_type == 8:
                        print(f"Client: {direction} ICMP Echo Request - {src_ip} -> {dest_ip}")
                    elif icmp_type == 0:
                        print(f"Client: {direction} ICMP Echo Reply - {src_ip} -> {dest_ip}")
                    elif icmp_type == 3:
                        print(f"Client: {direction} ICMP Dest Unreachable from {src_ip}")
                        
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error reading packet: {e}")

if __name__ == "__main__":
    main()
