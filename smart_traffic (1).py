import cv2
from ultralytics import YOLO
import numpy as np
import serial
import time
import tkinter as tk
from tkinter import filedialog

# --- CONFIGURATION ---
PORT_ESP = "COM47"
PORT_NANO = "COM45"  
BAUD_ESP = 115200
BAUD_NANO = 115200    

model = YOLO("yolov8n.pt")
vehicle_classes = [2, 3, 5, 7]  

# ====================== EMERGENCY TRACKING ======================
last_emergency_time = 0
EMERGENCY_COOLDOWN = 6  # 6 second cooldown between emergencies
last_emergency_message = ""

def get_video_paths():
    root = tk.Tk()
    root.withdraw()
    print("Select 3 videos for Road 1, 2, and 3")
    paths = [filedialog.askopenfilename(title=f"Select Video for Road {i}") for i in range(1, 4)]
    return [p for p in paths if p]

def analyze_road(path):
    cap = cv2.VideoCapture(path)
    if not cap.isOpened(): return 0
    
    line_y = 350
    vehicle_count = 0
    counted_ids = set()
    start_time = time.time()

    # Analyze for 5 seconds to get density
    while (time.time() - start_time) < 5:
        ret, frame = cap.read()
        if not ret: break
        frame = cv2.resize(frame, (640, 480))
        results = model.track(frame, persist=True, verbose=False)

        if results and results[0].boxes is not None and results[0].boxes.id is not None:
            boxes = results[0].boxes.xyxy.cpu().numpy()
            ids = results[0].boxes.id.cpu().numpy()
            classes = results[0].boxes.cls.cpu().numpy()

            for box, obj_id, cls in zip(boxes, ids, classes):
                if int(cls) not in vehicle_classes: continue
                y_center = int((box[1] + box[3]) / 2)
                if y_center > line_y and obj_id not in counted_ids:
                    vehicle_count += 1
                    counted_ids.add(obj_id)
        
        cv2.imshow("Analyzing Traffic Density", frame)
        if cv2.waitKey(1) == 27: break

    cap.release()
    cv2.destroyAllWindows()
    return vehicle_count

def set_all_red():
    """Set all lights to RED"""
    print("\n[SYSTEM] ⛔ Setting ALL ROADS to RED...")
    esp.write(f"SET:R1\n".encode())
    time.sleep(0.05)
    esp.write(f"SET:R2\n".encode())
    time.sleep(0.05)
    esp.write(f"SET:R3\n".encode())
    time.sleep(0.05)
    print("[SYSTEM] ✅ All roads are now RED\n")

# --- INITIALIZATION ---
video_list = get_video_paths()
if len(video_list) < 3:
    print("Error: 3 videos required.")
    exit()

esp = serial.Serial(PORT_ESP, BAUD_ESP, timeout=0.1)
nano = serial.Serial(PORT_NANO, BAUD_NANO, timeout=0.1)
time.sleep(2)

# --- MAIN LOOP ---
while True:
    # ===== SET ALL RED BEFORE ANALYZING =====
    set_all_red()
    time.sleep(1)
    
    # 1. Sense Density
    print("[SYSTEM] Analyzing Road Density...")
    counts = [analyze_road(v) for v in video_list]
    
    # 2. Calculate Timings (10s base + 2s per vehicle)
    # Each road has: 2s Yellow + Duration Green
    road_durations = [10 + (c * 2) for c in counts]
    yellow_time = 2
    
    start_cycle = time.time()
    active_sequence = True

    print(f"[LOG] Calculated Times: R1:{road_durations[0]}s, R2:{road_durations[1]}s, R3:{road_durations[2]}s")

    while active_sequence:
        elapsed = time.time() - start_cycle
        current_time = time.time()
        
        # ---- EMERGENCY RFID/SIREN DETECTION ----
        if nano.in_waiting > 0:
            line_nano = nano.readline().decode(errors='ignore').strip()
            
            # Only process if message is not empty and not a duplicate
            if line_nano and line_nano != last_emergency_message:
                if "AMBULANCE_CARD" in line_nano or "SIREN_DETECTED" in line_nano:
                    # Check cooldown period
                    if (current_time - last_emergency_time) >= EMERGENCY_COOLDOWN:
                        print(f"\n[🚨 EMERGENCY] {line_nano} - Adding 10s to active road.")
                        last_emergency_message = line_nano
                        last_emergency_time = current_time
                        
                        # Determine which road is currently active to add time
                        if elapsed < (road_durations[0] + yellow_time):
                            road_durations[0] += 10
                        elif elapsed < (sum(road_durations[:2]) + (yellow_time * 2)):
                            road_durations[1] += 10
                        else:
                            road_durations[2] += 10
                    else:
                        print(f"[COOLDOWN] Emergency cooldown active - ignoring duplicate signal")

        # ---- STATE MACHINE LOGIC ----
        # Determine which road/light should be ON
        if elapsed < (yellow_time + road_durations[0]):
            current_state = "Y1" if elapsed < yellow_time else "G1"
            rem = int((yellow_time + road_durations[0]) - elapsed)
        elif elapsed < (yellow_time * 2 + road_durations[0] + road_durations[1]):
            offset = yellow_time + road_durations[0]
            current_state = "Y2" if (elapsed - offset) < yellow_time else "G2"
            rem = int((offset + yellow_time + road_durations[1]) - elapsed)
        elif elapsed < (yellow_time * 3 + sum(road_durations)):
            offset = (yellow_time * 2) + road_durations[0] + road_durations[1]
            current_state = "Y3" if (elapsed - offset) < yellow_time else "G3"
            rem = int((offset + yellow_time + road_durations[2]) - elapsed)
        else:
            active_sequence = False
            continue

        # 3. Push State to ESP8266
        # Sending simplified commands: "SET:G1", "SET:Y2", etc.
        esp.write(f"SET:{current_state}\n".encode())
        
        print(f"\r[ACTIVE] Light: {current_state} | Time Remaining: {rem}s   ", end="", flush=True)
        time.sleep(0.1) 

    print("\n[SYSTEM] Cycle finished. Re-evaluating density...")