"use client";

import { useEffect, useRef, useState } from "react";
import { CAMERA_URLS } from "../panels/ReverseCameraPanel";

const RETRY_DELAY_MS = 2000;
const WIDTH = 220;
const ASPECT = 4 / 3;

const btnStyle: React.CSSProperties = {
  flex: 1,
  background: "rgba(255,255,255,0.12)",
  color: "#fff",
  border: "1px solid rgba(255,255,255,0.2)",
  borderRadius: 6,
  padding: "4px 0",
  fontSize: 13,
  cursor: "pointer",
};

export function CameraOverlay() {
  const [index, setIndex] = useState(0);
  const [minimized, setMinimized] = useState(false);
  const [retry, setRetry] = useState(0);
  const [failed, setFailed] = useState(false);
  const imgRef = useRef<HTMLImageElement>(null);

  const camera = CAMERA_URLS[index];
  const count = CAMERA_URLS.length;

  // Fresh connection whenever the selected camera changes
  useEffect(() => {
    setFailed(false);
    setRetry((r) => r + 1);
  }, [index]);

  // Auto-reconnect on failure
  useEffect(() => {
    if (!failed) return;
    const t = setTimeout(() => {
      setFailed(false);
      setRetry((r) => r + 1);
    }, RETRY_DELAY_MS);
    return () => clearTimeout(t);
  }, [failed]);

  // Close the HTTP connection on unmount / camera change
  useEffect(() => {
    const img = imgRef.current;
    return () => {
      if (img) img.src = "";
    };
  }, [retry, camera?.url]);

  const goPrev = () => setIndex((i) => (i > 0 ? i - 1 : count - 1));
  const goNext = () => setIndex((i) => (i < count - 1 ? i + 1 : 0));

  if (count === 0) return null;

  if (minimized) {
    return (
      <button
        onClick={() => setMinimized(false)}
        aria-label="Show camera overlay"
        style={{
          position: "absolute",
          top: 12,
          right: 12,
          zIndex: 20,
          background: "rgba(0,0,0,0.6)",
          color: "#fff",
          border: "1px solid rgba(255,255,255,0.25)",
          borderRadius: 8,
          padding: "6px 10px",
          fontSize: 13,
          cursor: "pointer",
        }}
      >
        &#128247; Show camera
      </button>
    );
  }

  return (
    <div
      style={{
        position: "absolute",
        top: 12,
        right: 12,
        zIndex: 20,
        width: WIDTH,
        background: "rgba(0,0,0,0.55)",
        borderRadius: 10,
        padding: 6,
        backdropFilter: "blur(4px)",
        color: "#fff",
      }}
    >
      <div
        style={{
          position: "relative",
          width: "100%",
          height: WIDTH / ASPECT,
          borderRadius: 6,
          overflow: "hidden",
          background: "#111",
        }}
      >
        {camera?.url && !failed && (
          <img
            ref={imgRef}
            key={`${camera.url}-${retry}`}
            src={camera.url}
            alt={camera.name}
            onError={() => setFailed(true)}
            style={{
              position: "absolute",
              inset: 0,
              width: "100%",
              height: "100%",
              objectFit: "cover",
            }}
          />
        )}
        <span
          style={{
            position: "absolute",
            left: 6,
            bottom: 4,
            fontSize: 11,
            fontWeight: 600,
            textShadow: "0 1px 3px rgba(0,0,0,0.8)",
          }}
        >
          {camera?.name ?? "No camera"}
          {failed ? " (reconnecting…)" : ""}
        </span>
      </div>

      <div style={{ display: "flex", gap: 4, marginTop: 6 }}>
        <button onClick={goPrev} aria-label="Previous camera" style={btnStyle}>
          &#10094;
        </button>
        <button
          onClick={() => setMinimized(true)}
          aria-label="Minimize camera overlay"
          style={{ ...btnStyle, flex: 1.4 }}
        >
          &#8211;
        </button>
        <button onClick={goNext} aria-label="Next camera" style={btnStyle}>
          &#10095;
        </button>
      </div>
    </div>
  );
}