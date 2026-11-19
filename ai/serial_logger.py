"""Save the gateway's LOG / EVT lines from the USB serial port to a CSV file.

The gateway prints one LOG line per minute (room state + number of manual
"AC on" presses). Leave this running during the field test; the CSV is the
training data for train_model.py.

    pip install pyserial
    python ai/serial_logger.py COM5            # Windows
    python ai/serial_logger.py /dev/ttyUSB0    # Linux / macOS
"""
import datetime
import os
import sys

import serial  # pyserial


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    port = sys.argv[1]
    os.makedirs(os.path.join(os.path.dirname(__file__), "data"), exist_ok=True)
    name = datetime.datetime.now().strftime("log_%Y%m%d_%H%M.csv")
    path = os.path.join(os.path.dirname(__file__), "data", name)
    with serial.Serial(port, 115200, timeout=1) as ser, open(path, "a", encoding="utf-8") as out:
        print(f"logging {port} -> {path}  (Ctrl+C to stop)")
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            if line.startswith(("LOG,", "EVT,")):
                out.write(line + "\n")
                out.flush()
            print(line)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
