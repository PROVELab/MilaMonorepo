import socket
import struct
import threading
import time
from typing import Dict, Tuple, Optional

import cv2
import numpy as np
import typer
from flask import Flask, Response

cli = typer.Typer(help="UDP video receiver with HTTP MJPEG streaming and FPS overlay")
flask_app = Flask(__name__)

# ---- Protocol / networking constants ----

HEADER_FORMAT = "!IHH"  # frame_id (uint32), total_fragments (uint16), fragment_index (uint16)
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

BUFFER_SIZE = 65535           # UDP recv buffer size
FRAME_TIMEOUT = 1.0           # seconds to keep incomplete frames
STALE_FRAME_TIMEOUT = 2.0     # if no new frames for this long, show placeholder in HTTP

# ---- Shared state between UDP thread and HTTP server ----

latest_jpeg: Optional[bytes] = None
latest_timestamp: float = 0.0
placeholder_jpeg: Optional[bytes] = None

latest_lock = threading.Lock()


class FrameBuffer:
    """Buffer for assembling one fragmented frame."""

    def __init__(self, total_fragments: int):
        self.total_fragments = total_fragments
        self.chunks = [None] * total_fragments  # type: ignore[list-item]
        self.received = 0
        self.last_update = time.time()

    def add_fragment(self, index: int, data: bytes):
        if 0 <= index < self.total_fragments and self.chunks[index] is None:
            self.chunks[index] = data
            self.received += 1
        self.last_update = time.time()

    def is_complete(self) -> bool:
        return self.received == self.total_fragments

    def assemble(self) -> bytes:
        return b"".join(chunk for chunk in self.chunks if chunk is not None)


def create_placeholder_jpeg(width: int = 1280, height: int = 720) -> bytes:
    """Create a simple 'Waiting for stream...' placeholder JPEG."""
    img = np.zeros((height, width, 3), dtype=np.uint8)

    text = "Waiting for stream..."
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = 2.0
    thickness = 3

    text_size, _ = cv2.getTextSize(text, font, font_scale, thickness)
    text_x = (width - text_size[0]) // 2
    text_y = (height // 2)

    cv2.putText(
        img,
        text,
        (text_x, text_y),
        font,
        font_scale,
        (255, 255, 255),
        thickness,
        cv2.LINE_AA,
    )

    success, encoded = cv2.imencode(".jpg", img, [int(cv2.IMWRITE_JPEG_QUALITY), 80])
    if not success:
        raise RuntimeError("Failed to create placeholder JPEG")

    return encoded.tobytes()


def udp_receiver(
    bind_host: str,
    port: int,
    show_preview: bool,
    jpeg_quality: int,
):
    """
    Receive fragmented JPEG frames over UDP, overlay FPS, optionally preview via OpenCV,
    and update global latest_jpeg for HTTP MJPEG streaming.
    """
    global latest_jpeg, latest_timestamp

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((bind_host, port))

    print(f"[UDP] Listening for video on {bind_host}:{port}")

    # Key: (addr, frame_id) -> FrameBuffer
    frames: Dict[Tuple[str, int], FrameBuffer] = {}

    # FPS tracking
    frame_count = 0
    last_fps_time = time.time()
    current_fps = 0.0

    encode_params = [int(cv2.IMWRITE_JPEG_QUALITY), int(jpeg_quality)]

    try:
        while True:
            try:
                packet, addr = sock.recvfrom(BUFFER_SIZE)
            except OSError as e:
                print(f"[UDP] recvfrom error: {e}")
                continue

            if len(packet) < HEADER_SIZE:
                continue

            try:
                header = packet[:HEADER_SIZE]
                payload = packet[HEADER_SIZE:]
                frame_id, total_fragments, fragment_index = struct.unpack(
                    HEADER_FORMAT, header
                )
            except struct.error:
                continue

            key = (addr[0], frame_id)

            buf = frames.get(key)
            if buf is None:
                buf = FrameBuffer(total_fragments)
                frames[key] = buf

            buf.add_fragment(fragment_index, payload)

            # Drop stale/incomplete frames
            now = time.time()
            for k in list(frames.keys()):
                if now - frames[k].last_update > FRAME_TIMEOUT:
                    del frames[k]

            # If frame complete, assemble and process
            if buf.is_complete():
                frame_bytes = buf.assemble()
                del frames[key]

                np_data = np.frombuffer(frame_bytes, dtype=np.uint8)
                frame = cv2.imdecode(np_data, cv2.IMREAD_COLOR)
                if frame is None:
                    continue

                # FPS tracking
                frame_count += 1
                elapsed = now - last_fps_time
                if elapsed >= 1.0:
                    current_fps = frame_count / elapsed
                    frame_count = 0
                    last_fps_time = now

                # Overlay FPS text
                fps_text = f"FPS: {current_fps:5.2f}"
                cv2.putText(
                    frame,
                    fps_text,
                    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    1.0,
                    (0, 255, 0),
                    2,
                    cv2.LINE_AA,
                )

                # Optional local preview
                if show_preview:
                    cv2.imshow("UDP Stream (Receiver Preview)", frame)
                    if cv2.waitKey(1) & 0xFF == ord("q"):
                        print("[UDP] Preview window closed by user.")
                        break

                # Re-encode with overlay for HTTP streaming
                success, encoded = cv2.imencode(".jpg", frame, encode_params)
                if not success:
                    continue

                with latest_lock:
                    latest_jpeg = encoded.tobytes()
                    latest_timestamp = time.time()

    except KeyboardInterrupt:
        print("\n[UDP] Stopping receiver (KeyboardInterrupt).")
    finally:
        sock.close()
        if show_preview:
            cv2.destroyAllWindows()
        print("[UDP] Receiver stopped.")


@flask_app.route("/stream")
def stream():
    """
    HTTP MJPEG endpoint that serves the latest JPEG frame (with FPS overlay)
    as a multipart MJPEG stream. Falls back to a placeholder frame when
    no recent frames are available, so the browser's <img> doesn't break.
    """

    def gen():
        while True:
            with latest_lock:
                frame = latest_jpeg
                ts = latest_timestamp
                placeholder = placeholder_jpeg

            now = time.time()
            # If we have no frame yet, or frames are stale, show placeholder
            use_placeholder = frame is None or (now - ts > STALE_FRAME_TIMEOUT)

            data = placeholder if use_placeholder and placeholder is not None else frame

            if data is None:
                # Shouldn't really happen once placeholder is set, but be safe
                time.sleep(0.05)
                continue

            yield (
                b"--frame\r\n"
                b"Content-Type: image/jpeg\r\n\r\n" +
                data +
                b"\r\n"
            )

            # Simple pacing to avoid tight loop burning CPU
            time.sleep(1.0 / 30.0)  # ~30 fps output for MJPEG

    return Response(gen(), mimetype="multipart/x-mixed-replace; boundary=frame")


@cli.command()
def run(
    bind_host: str = typer.Option("0.0.0.0", help="UDP bind host"),
    udp_port: int = typer.Option(5000, help="UDP port to listen for video"),
    http_host: str = typer.Option("127.0.0.1", help="HTTP server host"),
    http_port: int = typer.Option(8000, help="HTTP server port"),
    jpeg_quality: int = typer.Option(80, help="JPEG quality for HTTP stream (0-100)"),
    preview: bool = typer.Option(
        False, "--preview/--no-preview", help="Show local OpenCV preview window"
    ),
):
    """
    Start UDP receiver + HTTP MJPEG server.

    - UDP: listens for fragmented JPEG video frames, overlays FPS.
    - HTTP: serves MJPEG stream at /stream (always enabled).
      When sender stops, you see a placeholder; when sender restarts,
      video resumes without refreshing the page.
    """
    global placeholder_jpeg

    # Create placeholder frame once
    placeholder_jpeg = create_placeholder_jpeg()

    # Start UDP receiver thread
    udp_thread = threading.Thread(
        target=udp_receiver,
        args=(bind_host, udp_port, preview, jpeg_quality),
        daemon=True,
    )
    udp_thread.start()

    print(f"[HTTP] MJPEG stream available at http://{http_host}:{http_port}/stream")

    try:
        # Run Flask HTTP server (blocking)
        # IMPORTANT: disable reloader to avoid double-running threads
        flask_app.run(
            host=http_host,
            port=http_port,
            threaded=True,
            use_reloader=False,
        )
    except KeyboardInterrupt:
        print("\n[HTTP] HTTP server interrupted, shutting down.")
    finally:
        print("[MAIN] Exiting.")


if __name__ == "__main__":
    cli()
