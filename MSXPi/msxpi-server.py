import socket

HOST = '0.0.0.0'      # Listen on all interfaces
PORT = 5000           # Match this with serverPort in your C++ code

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen(1)
        print(f"[Python Server] Listening on {HOST}:{PORT}...")

        conn, addr = s.accept()
        with conn:
            print(f"[Python Server] Connected by {addr}")
            while True:
                data = conn.recv(1)  # Read one byte
                if data:
                    print(f"[Python Server] Received byte: {data[0]:02X}", flush=True)
                
                # Optional: echo back a response byte
                conn.sendall(bytes([(data[0] + 1) & 0xFF]))

if __name__ == "__main__":
    start_server()
