import socket
import json
import pyautogui

# غیرفعال کردن قابلیت FailSafe (اگر ماوس به گوشه‌های صفحه رفت، برنامه متوقف نشود)
pyautogui.FAILSAFE = False
# کاهش تاخیر پیش‌فرض PyAutoGUI برای حرکت بلادرنگ و سریع‌تر
pyautogui.PAUSE = 0.001

# تنظیمات شبکه
UDP_IP = "0.0.0.0"  # گوش دادن به تمامی اینترفیس‌های شبکه لپ‌تاپ
UDP_PORT = 5000

# تنظیمات فیلتر حرکتی (تنظیم بر اساس تست)
DEADZONE = 0.5  # لرزش‌های کمتر از این مقدار نادیده گرفته می‌شوند
SENSITIVITY = 1.5  # ضریب حساسیت حرکت ماوس

def main():
    # ایجاد سوکت UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    
    print(f"[SERVER] Air Mouse Server started on port {UDP_PORT}...")
    print("[SERVER] Waiting for mobile connection...")

    while True:
        try:
            # دریافت داده از شبکه
            data, addr = sock.recvfrom(1024)  # بافر ۱۰۲۴ بایتی
            
            # پارس کردن رشته JSON دریافتی
            packet = json.loads(data.decode('utf-8'))
            
            # ۱. مدیریت حرکت (Mouse Movement)
            # نگاشت: Z گوشی به X لپ‌تاپ، X گوشی به Y لپ‌تاپ
            delta_x = packet.get("DeltaX", 0.0) * SENSITIVITY
            delta_y = packet.get("DeltaY", 0.0) * SENSITIVITY
            
            # اعمال فیلتر آستانه لرزش (Deadzone)
            if abs(delta_x) < DEADZONE: delta_x = 0
            if abs(delta_y) < DEADZONE: delta_y = 0
            
            if delta_x != 0 or delta_y != 0:
                # جابه‌جایی نسبی مکان‌نما روی ویندوز
                pyautogui.moveRel(delta_x, delta_y)
            
            # ۲. مدیریت کلیک (Click Event)
            if packet.get("Click", False):
                pyautogui.click()
                print(f"[CLICK] Click executed. Sending ACK to {addr}")
                # ارسال تاییدیه (ACK) به گوشی
                ack_packet = json.dumps({"ACK": "Click"}).encode('utf-8')
                sock.sendto(ack_packet, addr)
                
            # ۳. مدیریت اسکرول (Scroll Event)
            # مقدار این فیلد می‌تواند ۱ (اسکرول به بالا) یا ۱- (اسکرول به پایین) باشد
            scroll_dir = packet.get("Scroll", 0)
            if scroll_dir != 0:
                # در PyAutoGUI مقادیر مثبت به بالا و منفی به پایین اسکرول می‌کنند
                pyautogui.scroll(scroll_dir * 100) 
                print(f"[SCROLL] Scroll executed ({scroll_dir}). Sending ACK to {addr}")
                # ارسال تاییدیه (ACK) به گوشی
                ack_packet = json.dumps({"ACK": "Scroll"}).encode('utf-8')
                sock.sendto(ack_packet, addr)

        except json.JSONDecodeError:
            print("[ERROR] Received invalid JSON format.")
        except Exception as e:
            print(f"[ERROR] An error occurred: {e}")

if __name__ == "__main__":
    main()