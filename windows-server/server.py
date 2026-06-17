import socket
import json
import pyautogui

# Disable FailSafe
pyautogui.FAILSAFE = False
# Reduce PyAutoGUI delay for instant response
pyautogui.PAUSE = 0.001

# Network settings
UDP_IP = "0.0.0.0"  # Listen on all interfaces
UDP_PORT = 5000

# Motion filter settings
DEADZONE = 0.5  # Ignore movement below threshold
SENSITIVITY = 1.5  # Mouse sensitivity multiplier

def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    
    print(f"[SERVER] Air Mouse Server started on port {UDP_PORT}...")
    print("[SERVER] Waiting for mobile connection...")

    while True:
        try:
            # Receive data from network
            data, addr = sock.recvfrom(1024)  # 1024 byte buffer
            
            # Parse received JSON
            packet = json.loads(data.decode('utf-8'))
            
            # 1. Mouse Movement
            # Phone Z→Laptop X, Phone X→Laptop Y
            delta_x = packet.get("DeltaX", 0.0) * SENSITIVITY
            delta_y = packet.get("DeltaY", 0.0) * SENSITIVITY
            
            # Apply deadzone filter
            if abs(delta_x) < DEADZONE: delta_x = 0
            if abs(delta_y) < DEADZONE: delta_y = 0
            
            if delta_x != 0 or delta_y != 0:
                # Move mouse relatively
                pyautogui.moveRel(delta_x, delta_y)
            
            # 2. Click Event
            if packet.get("Click", False):
                pyautogui.click()
                print(f"[CLICK] Click executed. Sending ACK to {addr}")
                # Send ACK to phone
                ack_packet = json.dumps({"ACK": "Click"}).encode('utf-8')
                sock.sendto(ack_packet, addr)
                
            # 3. Scroll Event
            # Value: 1 for up, -1 for down
            scroll_dir = packet.get("Scroll", 0)
            if scroll_dir != 0:
                # Positive = up, negative = down
                pyautogui.scroll(scroll_dir * 100) 
                print(f"[SCROLL] Scroll executed ({scroll_dir}). Sending ACK to {addr}")
                # Send ACK to phone
                ack_packet = json.dumps({"ACK": "Scroll"}).encode('utf-8')
                sock.sendto(ack_packet, addr)

        except json.JSONDecodeError:
            print("[ERROR] Received invalid JSON format.")
        except Exception as e:
            print(f"[ERROR] An error occurred: {e}")

if __name__ == "__main__":
    main()