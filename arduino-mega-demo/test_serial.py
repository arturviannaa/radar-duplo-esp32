import sys
import time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = 9600

def read_lines_for(ser, seconds):
    deadline = time.monotonic() + seconds
    lines = []
    buf = b""
    while time.monotonic() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            s = line.decode(errors="replace").strip()
            if s:
                lines.append(s)
                print(f"  <- {s}")
    return lines

def main():
    print(f"Abrindo {PORT} @ {BAUD}")
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    print("Esperando READY + heartbeats por 4s...")
    boot_lines = read_lines_for(ser, 4.0)

    saw_ready = any(l.startswith("READY") for l in boot_lines)
    hb_lines = [l for l in boot_lines if l.startswith("HB ")]

    print(f"\nEnviando 'ola' e esperando eco por 2s...")
    ser.write(b"ola\n")
    ser.flush()
    echo_lines = read_lines_for(ser, 2.0)
    saw_echo = any(l == "ECO: ola" for l in echo_lines)

    print("\n--- resultado ---")
    print(f"  READY recebido:      {saw_ready}")
    print(f"  heartbeats contados: {len(hb_lines)}")
    print(f"  ECO: ola recebido:   {saw_echo}")

    ok = saw_ready and len(hb_lines) >= 2 and saw_echo
    print("\nPASS" if ok else "\nFAIL")
    ser.close()
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
