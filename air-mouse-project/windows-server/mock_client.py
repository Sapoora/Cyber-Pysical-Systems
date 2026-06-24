import socket
import json
import time

SERVER_IP = "127.0.0.1"  # Localhost
SERVER_PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("[MOCK] Sending fake mouse data...")

# Simulate mouse movement
for i in range(50):
    # Send simulated movement data
    data = {"DeltaX": 5.0, "DeltaY": 3.0, "Click": False, "Scroll": 0}
    sock.sendto(json.dumps(data).encode('utf-8'), (SERVER_IP, SERVER_PORT))
    time.sleep(0.02) # 50Hz sensor simulation

# Test click event
print("[MOCK] Sending Click Event...")
click_data = {"DeltaX": 0.0, "DeltaY": 0.0, "Click": True, "Scroll": 0}
sock.sendto(json.dumps(click_data).encode('utf-8'), (SERVER_IP, SERVER_PORT))

# Wait for server ACK
sock.settimeout(2.0)
try:
    response, addr = sock.recvfrom(1024)
    print(f"[MOCK] Received ACK from server: {response.decode('utf-8')}")
except socket.timeout:
    print("[MOCK ERROR] No ACK received from server!")