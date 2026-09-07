import serial
import numpy as np
import cv2
import time

SERIAL_PORT = 'COM6'
BAUD_RATE = 2250000

WIDTH = 96
HEIGHT = 96
FRAME_SIZE = WIDTH * HEIGHT

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Successfully connected to 1-bit stream on {SERIAL_PORT}")
    except Exception as e:
        print(f"Error: {e}")
        return

    target_header = b"IMG_S"
    target_end = b"IMG_E"
    header_index = 0
    end_index = 0
    is_reading_frame = False
    is_reading_end = False

    frame_buffer = bytearray(FRAME_SIZE)
    frame_byte_count = 0
    last_time = time.time()

    def reset_state():
        nonlocal header_index, end_index, is_reading_frame, is_reading_end, frame_byte_count
        header_index = 0
        end_index = 0
        is_reading_frame = False
        is_reading_end = False
        frame_byte_count = 0

    while True:
        if ser.in_waiting > 0:
            data_chunk = ser.read(ser.in_waiting)
        else:
            data_chunk = ser.read(1)

        for in_byte in data_chunk:
            if not is_reading_frame and not is_reading_end:
                if in_byte == target_header[header_index]:
                    header_index += 1
                    if header_index == len(target_header):
                        is_reading_frame = True
                        frame_byte_count = 0
                        header_index = 0
                else:
                    header_index = 0
                    if in_byte == target_header[0]:
                        header_index = 1
            elif is_reading_frame:
                frame_buffer[frame_byte_count] = in_byte
                frame_byte_count += 1

                if frame_byte_count == FRAME_SIZE:
                    is_reading_frame = False
                    is_reading_end = True
                    end_index = 0
            else:
                if in_byte == target_end[end_index]:
                    end_index += 1
                    if end_index == len(target_end):
                        frame_raw = np.frombuffer(frame_buffer, dtype=np.uint8).reshape((HEIGHT, WIDTH))
                        frame_zoomed = cv2.resize(frame_raw, (WIDTH * 4, HEIGHT * 4), interpolation=cv2.INTER_NEAREST)

                        current_time = time.time()
                        time_diff = current_time - last_time
                        fps = 1.0 / time_diff if time_diff > 0 else 0.0
                        last_time = current_time

                        window_title = f"HM01B0 8-bits Parallel - FPS: {fps:.2f}"
                        cv2.imshow('HM01B0 8-Bits Parallel Grayscale', frame_zoomed)
                        cv2.setWindowTitle('HM01B0 8-Bits Parallel Grayscale', window_title)

                        key = cv2.waitKey(1) & 0xFF

                        # 按 'q' 離開
                        if key == ord('q'):
                            ser.close()
                            cv2.destroyAllWindows()
                            return

                        # 按 's' 儲存影像
                        elif key == ord('s'):
                            filename = f"unknown_{int(time.time())}.jpg"
                            cv2.imwrite(filename, frame_zoomed)
                            print(f"Image saved: {filename}")

                        reset_state()
                else:
                    reset_state()
                    if in_byte == target_header[0]:
                        header_index = 1

if __name__ == '__main__':
    main()