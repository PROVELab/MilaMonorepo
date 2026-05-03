export function ReverseCameraPanel() {
  return (
    <div className="reverse-camera">
      {/* eslint-disable-next-line @next/next/no-img-element */}
      <img src="http://127.0.0.1:8000/stream" alt="Reverse camera stream" />
    </div>
  );
}
