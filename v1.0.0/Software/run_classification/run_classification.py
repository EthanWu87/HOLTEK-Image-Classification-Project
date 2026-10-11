import serial
import numpy as np
import cv2
import time
import re

SERIAL_PORT = 'COM3'
BAUD_RATE = 2250000

WIDTH = 96
HEIGHT = 96
FRAME_SIZE = WIDTH * HEIGHT

def parse_prediction(text):
    dsp = 0
    classification = 0
    anomaly = 0
    labels = {}

    # -----------------------------
    # 解析 timing
    # -----------------------------
    timing_pattern = (
        r"Predictions\s*"
        r"\(DSP:\s*(\d+)\s*ms\.,\s*"
        r"Classification:\s*(\d+)\s*ms\.,\s*"
        r"Anomaly:\s*(\d+)\s*ms\.\)"
    )

    timing_match = re.search(timing_pattern, text)

    if timing_match:
        dsp = int(timing_match.group(1))
        classification = int(timing_match.group(2))
        anomaly = int(timing_match.group(3))

    # -----------------------------
    # 解析 label + confidence
    # -----------------------------
    label_pattern = r"^\s+([^:\r\n]+):\s*([0-9]*\.?[0-9]+)"

    for match in re.finditer(label_pattern, text, re.MULTILINE):
        label = match.group(1).strip()
        value = float(match.group(2))

        labels[label] = value

    return dsp, classification, anomaly, labels


def draw_prediction_panel(
    frame,
    dsp,
    classification,
    anomaly,
    labels,
    fps
):
    scale = 4

    frame_zoomed = cv2.resize(
        frame,
        (WIDTH * scale, HEIGHT * scale),
        interpolation=cv2.INTER_NEAREST
    )

    # OpenCV 顯示用 BGR
    frame_display = cv2.cvtColor(
        frame_zoomed,
        cv2.COLOR_GRAY2BGR
    )

    panel_width = 420
    panel_height = frame_display.shape[0]

    panel = np.zeros(
        (panel_height, panel_width, 3),
        dtype=np.uint8
    )

    y = 35

    cv2.putText(
        panel,
        "Classification Result",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.8,
        (255, 255, 255),
        2
    )

    y += 40

    # -----------------------------
    # FPS
    # -----------------------------
    cv2.putText(
        panel,
        f"FPS: {fps:.2f}",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (255, 255, 255),
        2
    )

    y += 40

    # -----------------------------
    # Timing
    # -----------------------------
    cv2.putText(
        panel,
        f"DSP:            {dsp} ms",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.6,
        (255, 255, 255),
        2
    )

    y += 30

    cv2.putText(
        panel,
        f"Classification:  {classification} ms",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.6,
        (255, 255, 255),
        2
    )

    y += 30

    cv2.putText(
        panel,
        f"Anomaly:         {anomaly} ms",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.6,
        (255, 255, 255),
        2
    )

    y += 45

    # -----------------------------
    # Label
    # -----------------------------
    cv2.putText(
        panel,
        "Classification:",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (255, 255, 255),
        2
    )

    y += 35

    # 找最高 confidence
    if labels:
        best_label = max(labels, key=labels.get)
        best_value = labels[best_label]
    else:
        best_label = "N/A"
        best_value = 0.0

    # -----------------------------
    # 顯示所有 label
    # -----------------------------
    for label, value in labels.items():

        percentage = value * 100

        text = f"{label:<12} {percentage:6.2f}%"

        # 最高 confidence 特別標記
        if label == best_label:

            cv2.putText(
                panel,
                text,
                (20, y),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                (0, 255, 0),
                2
            )

        else:

            cv2.putText(
                panel,
                text,
                (20, y),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                (255, 255, 255),
                2
            )

        y += 32

    y += 20

    # -----------------------------
    # 最終結果
    # -----------------------------
    cv2.putText(
        panel,
        "Result:",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.65,
        (255, 255, 255),
        2
    )

    y += 35

    cv2.putText(
        panel,
        best_label,
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.9,
        (0, 255, 0),
        2
    )

    y += 35

    cv2.putText(
        panel,
        f"{best_value * 100:.2f}%",
        (20, y),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (0, 255, 0),
        2
    )

    # -----------------------------
    # 合併影像 + panel
    # -----------------------------
    combined = np.hstack((frame_display, panel))

    return combined

def main():

    try:
        ser = serial.Serial(
            SERIAL_PORT,
            BAUD_RATE,
            timeout=1
        )

        print(
            f"Successfully connected to "
            f"Edge Impulse stream on {SERIAL_PORT}"
        )

    except Exception as e:
        print(f"Error: {e}")
        return

    # -----------------------------
    # 定義通訊狀態機的 States
    # -----------------------------
    STATE_FIND_IMG_S = 0
    STATE_READ_FRAME = 1
    STATE_FIND_IMG_E = 2
    STATE_FIND_AI_S  = 3
    STATE_READ_AI_TEXT = 4

    current_state = STATE_FIND_IMG_S

    target_header = b"IMG_S"
    target_end = b"IMG_E"
    ai_header = b"INF_S"
    ai_end = b"INF_E"

    img_s_idx = 0
    img_e_idx = 0
    ai_s_idx = 0
    ai_e_idx = 0

    frame_buffer = bytearray(FRAME_SIZE)
    frame_byte_count = 0
    
    prediction_buffer = bytearray()

    last_time = time.time()
    fps = 0.0

    # 最近一次 inference 結果
    dsp = 0
    classification = 0
    anomaly = 0
    labels = {}
    frame_raw = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)

    def process_prediction_text(buf):
        nonlocal dsp, classification, anomaly, labels
        try:
            text = buf.decode('ascii', errors='ignore')
            pos = text.find("Predictions")

            if pos < 0:
                return

            text = text[pos:]
            dsp, classification, anomaly, labels = parse_prediction(text)

            print("\n==============================")
            print(f"DSP:            {dsp} ms")
            print(f"Classification: {classification} ms")
            print(f"Anomaly:        {anomaly} ms")

            for label, value in labels.items():
                print(f"{label}: {value:.5f}")

            if labels:
                best_label = max(labels, key=labels.get)
                print(f"Result: {best_label} ({labels[best_label] * 100:.2f}%)")

            print("==============================")

        except Exception as e:
            print(f"Prediction parse error: {e}")

    while True:
        if ser.in_waiting > 0:
            data_chunk = ser.read(ser.in_waiting)
        else:
            data_chunk = ser.read(1)

        for in_byte in data_chunk:

            # =====================================================
            # 狀態 0：尋找影像開頭 IMG_S
            # =====================================================
            if current_state == STATE_FIND_IMG_S:
                if in_byte == target_header[img_s_idx]:
                    img_s_idx += 1
                    if img_s_idx == len(target_header):
                        current_state = STATE_READ_FRAME
                        frame_byte_count = 0
                        img_s_idx = 0
                else:
                    img_s_idx = 0
                    if in_byte == target_header[0]:
                        img_s_idx = 1

            # =====================================================
            # 狀態 1：接收 9216 bytes 影像資料
            # =====================================================
            elif current_state == STATE_READ_FRAME:
                frame_buffer[frame_byte_count] = in_byte
                frame_byte_count += 1

                if frame_byte_count == FRAME_SIZE:
                    current_state = STATE_FIND_IMG_E
                    img_e_idx = 0

            # =====================================================
            # 狀態 2：等待影像結尾 IMG_E
            # =====================================================
            elif current_state == STATE_FIND_IMG_E:
                if in_byte == target_end[img_e_idx]:
                    img_e_idx += 1
                    if img_e_idx == len(target_end):
                        # 影像接收完畢，轉換 Numpy 陣列並計算 FPS
                        frame_raw = np.frombuffer(
                            frame_buffer,
                            dtype=np.uint8
                        ).reshape((HEIGHT, WIDTH))

                        current_time = time.time()
                        time_diff = current_time - last_time
                        fps = (1.0 / time_diff) if time_diff > 0 else 0.0
                        last_time = current_time

                        # 繪製畫面與目前的 AI 結果
                        display = draw_prediction_panel(
                            frame_raw, dsp, classification, anomaly, labels, fps
                        )
                        cv2.imshow('HM01B0 + Edge Impulse', display)

                        # 影像完成後，切換去尋找 AI 預測開頭 AI_S
                        current_state = STATE_FIND_AI_S
                        ai_s_idx = 0
                else:
                    # 如果 IMG_E 匹配失敗，退回尋找 IMG_S
                    current_state = STATE_FIND_IMG_S
                    img_s_idx = (1 if in_byte == target_header[0] else 0)

            # =====================================================
            # 狀態 3：尋找 AI 預測開頭 AI_S
            # =====================================================
            elif current_state == STATE_FIND_AI_S:
                if in_byte == ai_header[ai_s_idx]:
                    ai_s_idx += 1
                    if ai_s_idx == len(ai_header):
                        current_state = STATE_READ_AI_TEXT
                        prediction_buffer.clear()
                        ai_e_idx = 0
                else:
                    ai_s_idx = 0
                    if in_byte == ai_header[0]:
                        ai_s_idx = 1

            # =====================================================
            # 状态 4：讀取 AI 預測文字直到 AI_E
            # =====================================================
            elif current_state == STATE_READ_AI_TEXT:
                prediction_buffer.append(in_byte)

                if in_byte == ai_end[ai_e_idx]:
                    ai_e_idx += 1
                    if ai_e_idx == len(ai_end):
                        # 完整接收到 AI_E，切除結尾的 "AI_E" 標籤並進行解析
                        clean_prediction_data = prediction_buffer[:-len(ai_end)]
                        process_prediction_text(clean_prediction_data)

                        # 一個完整的週期結束，回到開頭準備接收下一張圖 (IMG_S)
                        current_state = STATE_FIND_IMG_S
                        img_s_idx = 0
                else:
                    ai_e_idx = 0
                    if in_byte == ai_end[0]:
                        ai_e_idx = 1

        # OpenCV 鍵盤事件處理
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            ser.close()
            cv2.destroyAllWindows()
            return

if __name__ == '__main__':
    main()