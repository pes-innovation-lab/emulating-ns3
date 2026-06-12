import os
import socket

SERVER_IP = os.getenv("SERVER_IP", "10.99.1.11")
PORT = int(os.getenv("SERVER_PORT", "1234"))


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # disable checksum offloading allowing for udp echo to work (Linux only?)
    sock.setsockopt(socket.SOL_SOCKET, 11, 1)
    sock.settimeout(5)
    msg = b"Hello, World!"
    sock.sendto(msg, (SERVER_IP, PORT))
    data, addr = sock.recvfrom(36000)
    assert data == msg, f"Expected {msg!r}, got {data!r}"
    print(f"Recieved echo from {addr}: {data!r}")
    sock.close()


if __name__ == "__main__":
    main()
