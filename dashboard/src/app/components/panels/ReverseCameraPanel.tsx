export function ReverseCameraPanel() {
  return (
    <div className="panel panel--camera">
      <div className="panel__header">
        <h3>Reverse Camera</h3>
        <p>Feed placeholder (connect video stream later)</p>
      </div>
      <div className="camera-placeholder">
        <div className="camera-placeholder__grid" />
        <span>Awaiting video signal…</span>
      </div>
    </div>
  );
}
