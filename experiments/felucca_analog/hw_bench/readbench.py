import serial, time, sys, os
import glob
PORT = (glob.glob('/dev/ttyACM*') or ['/dev/ttyACM0'])[0]
out, wait = sys.argv[1], float(sys.argv[2])
end = time.time() + wait
buf = b''
while time.time() < end and b'DONE' not in buf:
    try:
        if not os.path.exists(PORT):
            time.sleep(0.2); continue
        s = serial.Serial(); s.port = PORT; s.baudrate = 115200; s.timeout = 0.5
        s.dtr = False; s.rts = False; s.open()
        s.rts = True; time.sleep(0.15); s.rts = False
        while time.time() < end and b'DONE' not in buf:
            buf += s.read(4096)
        s.close()
    except (serial.SerialException, OSError) as e:
        time.sleep(0.3)
open(out, 'wb').write(buf)
print(len(buf), 'bytes')
