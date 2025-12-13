import socket
import struct
import time
from typing import Optional

import cv2
import typer

app = typer.Typer(help="Simplified UDP video sender with FPS pacing")

MTU = 1400  # bytes
HEADER_FMT = "!IHH"  # frame_id, total_frags, frag_idx
HEADER_SIZE = struct.calcsize(HEADER_FMT)
PAYLOAD_SIZE = MTU - HEADER_SIZE


@app.command()
def send(
    host: str = typer.Argument("127.0.0.1"),
    port: int = typer.Argument(5000),
    video: str = typer.Option(..., "--video", "-v", help="Path to MP4"),
    jpeg_quality: int = typer.Option(70, help="JPEG quality 0-100"),
    fps_override: float = typer.Option(
        0.0,
        "--fps",
        help="Override FPS (0 = use FPS from file)",
    ),
):
    """
    Send a 1920x1080 video over UDP using fragmented JPEG frames,
    paced to the video's FPS (or an override).
    """

    cap = cv2.VideoCapture(video)
    if not cap.isOpened():
        raise RuntimeError(f"Could not open video file: {video}")

    # Determine FPS
    if fps_override > 0:
        fps = fps_override
    else:
        video_fps = cap.get(cv2.CAP_PROP_FPS)
        fps = video_fps if video_fps and video_fps > 0 else 30.0

    frame_interval = 1.0 / fps
    print(f"Streaming {video} → {host}:{port} at ~{fps:.2f} FPS")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    addr = (host, port)
    encode_params = [int(cv2.IMWRITE_JPEG_QUALITY), int(jpeg_quality)]

    frame_id = 0
    next_frame_time = time.perf_counter()

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("End of video file.")
                break

            # Assume input is already 1920x1080; no resizing

            # Encode JPEG
            success, encoded = cv2.imencode(".jpg", frame, encode_params)
            if not success:
                print("Failed to encode frame, skipping")
                continue

            data = encoded.tobytes()
            total_frags = (len(data) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE

            # Send fragments
            for frag_idx in range(total_frags):
                start = frag_idx * PAYLOAD_SIZE
                end = start + PAYLOAD_SIZE
                chunk = data[start:end]

                header = struct.pack(HEADER_FMT, frame_id, total_frags, frag_idx)
                sock.sendto(header + chunk, addr)

            frame_id = (frame_id + 1) & 0xFFFFFFFF

            # FPS pacing
            next_frame_time += frame_interval
            sleep_for = next_frame_time - time.perf_counter()
            if sleep_for > 0:
                time.sleep(sleep_for)
            else:
                # We're behind; reset schedule to avoid drift
                next_frame_time = time.perf_counter()

    finally:
        cap.release()
        sock.close()
        print("Sender finished.")


if __name__ == "__main__":
    app()
