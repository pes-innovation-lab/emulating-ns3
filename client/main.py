import socket
import struct

CLIENT_MAC = "00:00:00:00:00:02"

def main():
    # Bind to raw socket for ARP (0x0806)
    ETH_P_ARP = 0x0806
    # socket.htons(ETH_P_ARP) is 0x0608
    s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(ETH_P_ARP))
    
    # Bind to the nk2 interface with a retry loop to wait for interface creation
    import time
    while True:
        try:
            s.bind(("nk2", ETH_P_ARP))
            print("Successfully bound to nk2.")
            break
        except Exception as e:
            print(f"Waiting for interface nk2 to be ready... ({e})")
            time.sleep(1)

    print("Client listening for ARP packets on nk2...")
    while True:
        try:
            packet, addr = s.recvfrom(2048)
            # Parse Ethernet header (first 14 bytes)
            if len(packet) < 14:
                continue
            eth_header = packet[:14]
            dest_mac, src_mac, eth_type = struct.unpack("!6s6sH", eth_header)
            
            # Parse ARP header (starts at byte 14, size 28 bytes)
            arp_header = packet[14:42]
            if len(arp_header) < 28:
                continue
                
            hw_type, proto_type, hw_size, proto_size, op = struct.unpack("!HHBBH", arp_header[:8])
            
            # Parse IPs and MACs
            src_mac_addr = struct.unpack("!6B", arp_header[8:14])
            src_ip = socket.inet_ntoa(arp_header[14:18])
            dest_mac_addr = struct.unpack("!6B", arp_header[18:24])
            dest_ip = socket.inet_ntoa(arp_header[24:28])
            
            src_mac_str = ":".join(f"{b:02x}" for b in src_mac_addr)
            dest_mac_str = ":".join(f"{b:02x}" for b in dest_mac_addr)
            
            is_sent = (src_mac_str.lower() == CLIENT_MAC.lower())
            direction = "Sent" if is_sent else "Received"
            
            if op == 1:
                print(f"Client: {direction} ARP Request - Who has {dest_ip}? Tell {src_ip}")
            elif op == 2:
                print(f"Client: {direction} ARP Response - {src_ip} is at {src_mac_str}")
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error reading packet: {e}")

if __name__ == "__main__":
    main()
