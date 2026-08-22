import cv2
import numpy as np
import sys
import json

def nothing(x):
    pass

if len(sys.argv) < 2:
    print("Usage: python tuner.py <video_path>")
    sys.exit(-1)

video_path = sys.argv[1]
cap = cv2.VideoCapture(video_path)

if not cap.isOpened():
    print("Error opening video")
    sys.exit(-1)

cv2.namedWindow('HSV_Tuner', cv2.WINDOW_NORMAL)
cv2.resizeWindow('HSV_Tuner', 300, 250)
cv2.createTrackbar('H Min', 'HSV_Tuner', 0, 180, nothing)
cv2.createTrackbar('H Max', 'HSV_Tuner', 60, 180, nothing)
cv2.createTrackbar('S Min', 'HSV_Tuner', 84, 255, nothing)
cv2.createTrackbar('S Max', 'HSV_Tuner', 255, 255, nothing)
cv2.createTrackbar('V Min', 'HSV_Tuner', 150, 255, nothing)
cv2.createTrackbar('V Max', 'HSV_Tuner', 255, 255, nothing)
cv2.createTrackbar('Morph_K', 'HSV_Tuner', 7, 20, nothing)

paused = False

while True:
    if not paused:
        ret, frame = cap.read()
        if not ret:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue
            
    h_min = cv2.getTrackbarPos('H Min', 'HSV_Tuner')
    h_max = cv2.getTrackbarPos('H Max', 'HSV_Tuner')
    s_min = cv2.getTrackbarPos('S Min', 'HSV_Tuner')
    s_max = cv2.getTrackbarPos('S Max', 'HSV_Tuner')
    v_min = cv2.getTrackbarPos('V Min', 'HSV_Tuner')
    v_max = cv2.getTrackbarPos('V Max', 'HSV_Tuner')
    k = max(1, cv2.getTrackbarPos('Morph_K', 'HSV_Tuner'))
    
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    lower = np.array([h_min, s_min, v_min])
    upper = np.array([h_max, s_max, v_max])
    mask = cv2.inRange(hsv, lower, upper)
    
    kernel = np.ones((k, k), np.uint8)
    mask_dilated = cv2.dilate(mask, kernel, iterations=1)
    
    # 设置窗口大小
    cv2.namedWindow('Original', cv2.WINDOW_NORMAL)
    cv2.namedWindow('Mask_Dilated', cv2.WINDOW_NORMAL)
    cv2.resizeWindow('Original', 640, 480)
    cv2.resizeWindow('Mask_Dilated', 640, 480)
    
    cv2.imshow('Original', frame)
    cv2.imshow('Mask_Dilated', mask_dilated)
    
    key = cv2.waitKey(30) & 0xFF
    if key == ord('q') or key == 27:
        # 退出前询问是否保存参数
        save_choice = input("\nSave current parameters? (y/n): ")
        if save_choice.lower() == 'y':
            params = {
                'h_min': h_min,
                'h_max': h_max,
                's_min': s_min,
                's_max': s_max,
                'v_min': v_min,
                'v_max': v_max,
                'kernel_size': k
            }
            with open('hsv_params.json', 'w') as f:
                json.dump(params, f, indent=4)
            print(f"Parameters saved to hsv_params.json:")
            print(f"H: [{h_min}, {h_max}]")
            print(f"S: [{s_min}, {s_max}]")
            print(f"V: [{v_min}, {v_max}]")
            print(f"Morph_K: {k}")
        break
    elif key == ord(' '):
        paused = not paused

cap.release()
cv2.destroyAllWindows()