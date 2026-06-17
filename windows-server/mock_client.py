import socket
import json
import time

SERVER_IP = "127.0.0.1"  # تست روی سیستم خودتان (Localhost)
SERVER_PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("[MOCK] Sending fake mouse data...")

# تست حرکت ماوس به صورت دایره‌ای یا رفت و برگشتی
for i in range(50):
    # فرض کنید گوشی کمی به سمت راست و پایین حرکت کرده است
    data = {"DeltaX": 5.0, "DeltaY": 3.0, "Click": False, "Scroll": 0}
    sock.sendto(json.dumps(data).encode('utf-8'), (SERVER_IP, SERVER_PORT))
    time.sleep(0.02) # شبیه‌سازی فرکانس ۵۰ هرتز سنسور

# تست ارسال کلیک
print("[MOCK] Sending Click Event...")
click_data = {"DeltaX": 0.0, "DeltaY": 0.0, "Click": True, "Scroll": 0}
sock.sendto(json.dumps(click_data).encode('utf-8'), (SERVER_IP, SERVER_PORT))

# گوش دادن به پاسخ سرور برای دریافت ACK
sock.settimeout(2.0)
try:
    response, addr = sock.recvfrom(1024)
    print(f"[MOCK] Received ACK from server: {response.decode('utf-8')}")
except socket.timeout:
    print("[MOCK ERROR] No ACK received from server!")