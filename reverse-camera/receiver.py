import cv2 as cv
import socket, struct, time
from collections import defaultdict
import numpy as np

LISTEN_IP   = "0.0.0.0"
LISTEN_PORT = 5004
PAYLOAD_PT  = 96
MAX_JITTER_BUFFER_MS = 20
FRAME_TIMEOUT_MS     = 60
RECV_BUF_BYTES       = 1<<20
WINDOW_NAME          = "MJPEG (RTP)"

class StreamState:
    def __init__(self):
        self.current_ts = None
        self.packets = []
        self.complete = False
        self.last_update = time.monotonic()
        self.frames = 0
        self.last_fps = time.monotonic()
        self.ema_fps = None
        self.alpha = 0.2

def parse_rtp(pkt):
    if len(pkt) < 12: return None
    first, seq, ts, ssrc = struct.unpack("!H H I I", pkt[:12])
    if ((first >> 14) & 0x3) != 2: return None
    m  = (first >> 7) & 0x1
    pt = first & 0x7F
    return seq, ts, ssrc, m, pt, pkt[12:]

def maybe_display(st: StreamState):
    if not st.complete:
        st.current_ts = None; st.packets.clear(); return
    img = cv.imdecode(np.frombuffer(b"".join(st.packets), np.uint8), cv.IMREAD_COLOR)
    st.current_ts = None; st.packets.clear(); st.complete = False
    if img is None or img.size == 0: return

    st.frames += 1
    now = time.monotonic()
    if now - st.last_fps >= 1.0:
        inst = st.frames / (now - st.last_fps)
        st.ema_fps = inst if st.ema_fps is None else (st.alpha*inst + (1-st.alpha)*st.ema_fps)
        st.frames = 0; st.last_fps = now

    fps = st.ema_fps or 0.0
    cv.putText(img, f"Receiver FPS: {fps:.1f}", (10,30),
               cv.FONT_HERSHEY_SIMPLEX, 1.0, (0,255,0), 2, cv.LINE_AA)
    cv.setWindowTitle(WINDOW_NAME, f"{WINDOW_NAME} — FPS: {fps:.1f}")
    cv.imshow(WINDOW_NAME, img)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, RECV_BUF_BYTES)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    sock.settimeout(0.02)

    streams = defaultdict(StreamState)
    cv.namedWindow(WINDOW_NAME, cv.WINDOW_NORMAL)

    try:
        while True:
            try:
                while True:
                    pkt,_ = sock.recvfrom(65535)
                    parsed = parse_rtp(pkt)
                    if not parsed: continue
                    seq, ts, ssrc, m, pt, payload = parsed
                    if pt != PAYLOAD_PT: continue
                    st = streams[ssrc]
                    now = time.monotonic()
                    if st.current_ts is None or ts != st.current_ts:
                        if st.current_ts is not None: maybe_display(st)
                        st.current_ts = ts; st.packets = [payload]; st.complete = bool(m); st.last_update = now
                    else:
                        st.packets.append(payload); st.last_update = now
                        if m: st.complete = True
            except socket.timeout:
                pass

            for st in list(streams.values()):
                if st.current_ts is None: continue
                age = (time.monotonic() - st.last_update) * 1000
                if st.complete or age > MAX_JITTER_BUFFER_MS: maybe_display(st)
                elif age > FRAME_TIMEOUT_MS: st.current_ts=None; st.packets.clear()

            # GUI poll — keep tiny
            if cv.waitKey(1) == 27: break

    finally:
        sock.close()
        cv.destroyAllWindows()

if __name__ == "__main__":
    main()
