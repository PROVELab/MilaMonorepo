import React from "react";

export function ReverseCameraPanel() {
  return (
    <div
      style={{
        width: "100%",
        height: "100%",        // fills parent area
        overflow: "hidden",
        backgroundColor: "black",
      }}
    >
      <img
        src="http://127.0.0.1:8000/stream"
        alt="UDP Stream"
        style={{
          width: "100%",
          height: "100%",      // stretch to container in both directions
          objectFit: "contain", // or "cover" if you prefer cropping
          display: "block",    // removes small bottom gap in some browsers
        }}
      />
    </div>
  );
}
