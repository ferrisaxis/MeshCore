import serial, time, sys, re

PORT = '/dev/ttyACM0'

def frame(cmd, text=None):
    payload = bytes([cmd]) + (text.encode() if text else b'')
    return b'<' + bytes([len(payload) & 0xFF, len(payload) >> 8]) + payload

def read_frames(s, timeout=2.0):
    out = []
    buf = b''
    end = time.time() + timeout
    while time.time() < end:
        buf += s.read(256)
        while True:
            i = buf.find(b'>')
            if i < 0 or len(buf) < i + 3:
                break
            ln = buf[i + 1] | (buf[i + 2] << 8)
            if len(buf) < i + 3 + ln:
                break
            out.append(buf[i + 3:i + 3 + ln])
            buf = buf[i + 3 + ln:]
        time.sleep(0.05)
    return out

def cli(cmd, wait=1.5):
    with serial.Serial(PORT, 115200, timeout=0.3) as s:
        time.sleep(0.4)
        s.reset_input_buffer()
        s.write(frame(66, cmd))
        frames = read_frames(s, wait)
    for f in frames:
        if f and f[0] == 29:
            return f[1:].decode('utf-8', 'replace')
    return '(no CLI reply; frames: %r)' % [f[:20] for f in frames]

def handshake():
    with serial.Serial(PORT, 115200, timeout=0.3) as s:
        time.sleep(0.4)
        s.reset_input_buffer()
        s.write(b'\x01')
        frames = read_frames(s, 2.0)
    return frames

if __name__ == '__main__':
    cmd = sys.argv[1]
    print(cli(' '.join(sys.argv[2:]) if len(sys.argv) > 2 else cmd))
