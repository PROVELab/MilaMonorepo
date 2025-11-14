import cv2 as cv
import socket, struct, time, random, os
from collections import deque

# === CONFIG ===
VIDEO_PATH      = "./drone_tutorial.mp4"      # <--- set this
DEST_IP         = "127.0.0.1"
DEST_PORT       = 5004
FPS_OVERRIDE    = None # e.g. 60.0, else use file FPS
JPEG_QUALITY    = 80
PAYLOAD_PT      = 96
MTU_PAYLOAD     = 1400       # bigger chunk -> fewer sendto()
USE_TURBOJPEG   = True       # pip install PyTurboJPEG
ENABLE_PREVIEW  = False
# ==============

# Try TurboJPEG
_tj = None
# if USE_TURBOJPEG:
#     try:
from turbojpeg import TurboJPEG, TJPF_BGR, TJSAMP_420
_tj = TurboJPEG()
    # except Exception:
    #     print()
    #     _tj = None

def build_rtp_header(seq, timestamp, ssrc, marker=False, payload_type=PAYLOAD_PT):
    v,p,x,cc = 2,0,0,0
    m = 1 if marker else 0
    first = (v<<14) | (p<<13) | (x<<12) | (cc<<8) | (m<<7) | (payload_type & 0x7F)
    return struct.pack("!H H I I", first, seq & 0xFFFF, timestamp & 0xFFFFFFFF, ssrc & 0xFFFFFFFF)

def encode_jpeg(frame):
    if _tj is not None:
        return _tj.encode(frame, pixel_format=TJPF_BGR, jpeg_subsample=TJSAMP_420, quality=JPEG_QUALITY)
    ok, buf = cv.imencode(".jpg", frame, [int(cv.IMWRITE_JPEG_QUALITY), int(JPEG_QUALITY)])
    return buf.tobytes() if ok else None

def main():
    if not os.path.exists(VIDEO_PATH):
        raise FileNotFoundError(VIDEO_PATH)
    cap = cv.VideoCapture(VIDEO_PATH)
    if not cap.isOpened():
        raise RuntimeError(f"Could not open video file: {VIDEO_PATH}")

    file_fps = cap.get(cv.CAP_PROP_FPS) or 30.0
    target_fps = FPS_OVERRIDE or file_fps
    frame_period = 1.0 / max(1e-6, target_fps)
    print(f"[Sender] Source FPS≈{file_fps:.2f}, Target FPS≈{target_fps:.2f}, TurboJPEG={'on' if _tj else 'off'}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1<<20)

    seq = 0
    ssrc = random.getrandbits(32)
    ts_inc = int(90000 / max(1e-6, target_fps))
    timestamp = random.randrange(0, 1<<32)

    if ENABLE_PREVIEW:
        cv.namedWindow("Sender Preview", cv.WINDOW_NORMAL)

    # pacing based on a moving “next deadline” (compensates for work time)
    next_deadline = time.perf_counter()

    sent_frames = 0
    last_fps_t = time.monotonic()
    ema_fps = None
    alpha = 0.2

    try:
        while True:
            # pace
            now = time.perf_counter()
            if now < next_deadline:
                time.sleep(next_deadline - now)
            next_deadline += frame_period

            ok, frame = cap.read()
            if not ok:
                # loop
                cap.set(cv.CAP_PROP_POS_FRAMES, 0)
                continue

            t0 = time.monotonic()
            jpg = encode_jpeg(frame)
            if not jpg:
                continue

            # packetize – fewer, bigger packets
            n = len(jpg)
            off = 0
            while off < n:
                chunk = jpg[off:off+MTU_PAYLOAD]
                off += len(chunk)
                marker = (off >= n)
                hdr = build_rtp_header(seq, timestamp, ssrc, marker, PAYLOAD_PT)
                sock.sendto(hdr + chunk, (DEST_IP, DEST_PORT))
                seq = (seq + 1) & 0xFFFF

            timestamp = (timestamp + ts_inc) & 0xFFFFFFFF
            sent_frames += 1

            # fps report
            now = time.monotonic()
            if now - last_fps_t >= 1.0:
                inst = sent_frames / (now - last_fps_t)
                ema_fps = inst if ema_fps is None else (alpha*inst + (1-alpha)*ema_fps)
                print(f"[Sender] FPS: {inst:.1f} (EMA {ema_fps:.1f})  enc+send ~{(now - t0)*1000:.1f} ms")
                sent_frames = 0
                last_fps_t = now

            if ENABLE_PREVIEW:
                disp = frame.copy()
                cv.putText(disp, f"Sender FPS: {ema_fps or 0:.1f}", (10,30),
                           cv.FONT_HERSHEY_SIMPLEX, 1.0, (0,255,0), 2, cv.LINE_AA)
                cv.imshow("Sender Preview", disp)
                cv.setWindowTitle("Sender Preview", f"Sender Preview — FPS: {ema_fps or 0:.1f}")
                if cv.waitKey(1) == 27: break

    finally:
        cap.release()
        sock.close()
        if ENABLE_PREVIEW:
            cv.destroyAllWindows()

if __name__ == "__main__":
    main()
