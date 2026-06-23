import socket
import time
import sys

def main():
    print("SCTP Client starting...", flush=True)
    # Wait for the nk2 interface to be assigned IP 10.10.0.2
    bound = False
    for i in range(30):
        try:
            s_test = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP)
            s_test.bind(('10.10.0.2', 0))
            s_test.close()
            bound = True
            print("Successfully bound test socket to 10.10.0.2. Interface is ready!", flush=True)
            break
        except Exception as e:
            print(f"Waiting for 10.10.0.2 interface to be ready... ({e})", flush=True)
            time.sleep(1)
            
    if not bound:
        print("Error: Interface 10.10.0.2 not ready. Exiting.")
        sys.exit(1)

    # Now, try connecting to the SCTP server at 10.10.0.1:5001
    connected = False
    client_socket = None
    for i in range(120):
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP)
            client_socket.bind(('10.10.0.2', 0))
            client_socket.settimeout(2.0)
            client_socket.connect(('10.10.0.1', 5001))
            client_socket.settimeout(None)
            connected = True
            print("Connected to NS-3 SCTP Server successfully!", flush=True)
            break
        except Exception as e:
            print(f"Waiting for NS-3 SCTP Server at 10.10.0.1:5001... ({e})", flush=True)
            if client_socket:
                client_socket.close()
            time.sleep(1)

    if not connected:
        print("Error: Could not connect to SCTP Server. Exiting.")
        sys.exit(1)

    try:
        # Send payload
        payload = b"Hello from Client via SCTP"
        client_socket.sendall(payload)
        print(f"Client: Sent SCTP payload \"{payload.decode()}\"", flush=True)

        # Receive reply
        data = client_socket.recv(1024)
        if data:
            print(f"Client: Received SCTP payload: \"{data.decode()}\"", flush=True)
        else:
            print("Client: Received empty reply / connection closed.", flush=True)
    except Exception as e:
        print(f"Error during SCTP exchange: {e}", flush=True)
    finally:
        client_socket.close()
        print("Client socket closed.", flush=True)

    print("Client keeping container alive. Press Ctrl+C to stop.", flush=True)
    while True:
        try:
            time.sleep(3600)
        except KeyboardInterrupt:
            break

if __name__ == '__main__':
    main()
