#!/usr/bin/env python3
"""
wait_port.py – Chờ port TCP sẵn sàng
Dùng: python3 wait_port.py <port> <timeout_seconds>
Exit code: 0 = port sẵn sàng, 1 = timeout
"""
import sys, socket, time

port = int(sys.argv[1])
timeout = int(sys.argv[2])
start = time.time()

while time.time() - start < timeout:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1)
        s.connect(("127.0.0.1", port))
        s.close()
        print(f"Port {port} ready after {time.time()-start:.1f}s")
        sys.exit(0)
    except:
        time.sleep(0.5)

print(f"Port {port} not ready after {timeout}s")
sys.exit(1)
