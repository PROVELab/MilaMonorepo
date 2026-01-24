export function ReverseCameraPanel() {
  return (
    <div className="reverse-camera">
      <img
        src="http://127.0.0.1:8000/stream"
        alt="UDP Stream"
        className="reverse-camera__feed"
      />
    </div>
  );
}
