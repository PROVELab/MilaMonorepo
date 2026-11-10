"use client";

import { useEffect, useRef, useState } from "react";

// ---- Config -----------------------------------------------------------
// Sorry for vibecoding
//The length of this list sets the cap for n.
//You can add or remove any mjpeg urls
//You can find more mjpeg urls at https://www.opentopia.com/hiddencam.php
interface CameraSource {
  name: string;
  url: string;
}
const CAMERA_URLS: readonly CameraSource[] = [
  { name: "Honjin Intersection", url: "http://honjin1.miemasu.net/nphMotionJpeg?Resolution=640x480&Quality=Standard" },
  { name: "Static Cam", url: "http://97.76.165.226/mjpg/video.mjpg" },
  { name: "ERAU Flight Line", url: "http://flightcam1.pr.erau.edu/mjpg/video.mjpg" },
  { name: "Sanford Counter", url: "https://mjpeg.sanford.io/count.mjpeg" },
];

const ASPECT = 16 / 9;      // width / height of every cell
const GAP = 8;              // px between cells
const BOTTOM_OFFSET = 110;  // px reserved for the bottom nav bar; tweak to fit
const RETRY_DELAY_MS = 2000;
const MAX_CAMERAS = Math.max(1, CAMERA_URLS.length);

// ---- Helpers ----------------------------------------------------------

// Golden-angle hue step: neighbors stay distinct and each tile keeps its color
const cameraColor = (i: number) => `hsl(${(i * 137.5) % 360}, 70%, 45%)`;

// Try every column count and keep the one that gives the largest cells
function bestGrid(n: number, w: number, h: number) {
  let best = { cols: 1, rows: n, cellW: 0 };
  for (let cols = 1; cols <= n; cols++) {
    const rows = Math.ceil(n / cols);
    const maxW = (w - (cols - 1) * GAP) / cols;
    const maxH = (h - (rows - 1) * GAP) / rows;
    const cellW = Math.min(maxW, maxH * ASPECT);
    if (cellW > best.cellW) best = { cols, rows, cellW };
  }
  return best;
}

// ---- Camera tile ------------------------------------------------------

function CameraTile({
  index,
  name,
  url,
}: {
  index: number;
  name?: string;
  url?: string;
}) {
  const imgRef = useRef<HTMLImageElement>(null);
  const [retry, setRetry] = useState(0);
  const [failed, setFailed] = useState(false);

  const label = name ?? `Camera ${index + 1}`;

  useEffect(() => {
    if (!failed) return;
    const t = setTimeout(() => {
      setFailed(false);
      setRetry((r) => r + 1);
    }, RETRY_DELAY_MS);
    return () => clearTimeout(t);
  }, [failed]);

  useEffect(() => {
    const img = imgRef.current;
    return () => {
      if (img) img.src = "";
    };
  }, [retry, url]);

  return (
    <div
      style={{
        position: "relative",
        background: cameraColor(index),
        overflow: "hidden",
      }}
    >
      {url && !failed && (
        <img
          ref={imgRef}
          key={`${url}-${retry}`}
          src={url}
          alt={label}
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
          left: 8,
          top: 8,
          fontWeight: 600,
          textShadow: "0 1px 3px rgba(0,0,0,0.6)",
        }}
      >
        {label}
        {!url ? " (no source)" : failed ? " (reconnecting…)" : ""}
      </span>
    </div>
  );
}

// ---- Panel ------------------------------------------------------------

export function ReverseCameraPanel() {
  const [n, setN] = useState(MAX_CAMERAS);
  const [size, setSize] = useState({ w: 0, h: 0 });
  const areaRef = useRef<HTMLDivElement>(null);

  // Measure the grid area (its size is set explicitly below, so this is reliable)
  useEffect(() => {
    const el = areaRef.current;
    if (!el) return;
    const ro = new ResizeObserver(([entry]) => {
      const { width, height } = entry.contentRect;
      setSize({ w: width, h: height });
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, []);

  const { cols, cellW } = bestGrid(n, size.w, size.h);
  const cellH = cellW / ASPECT;

  return (
    <div
      style={{
        display: "flex",
        flexDirection: "column",
        width: "100vw",
        height: `calc(100vh - ${BOTTOM_OFFSET}px)`,
        boxSizing: "border-box",
        color: "#fff",
      }}
    >
      <label
        style={{ display: "flex", gap: 12, alignItems: "center", padding: 8 }}
      >
        <span style={{ minWidth: 48 }}>n = {n}</span>
        <input
          type="range"
          min={1}
          max={MAX_CAMERAS}
          value={n}
          onChange={(e) => setN(Number(e.target.value))}
          style={{ flex: 1 }}
        />
      </label>

      <div ref={areaRef} style={{ flex: 1, minHeight: 0, minWidth: 0 }}>
        <div
          style={{
            display: "grid",
            gridTemplateColumns: `repeat(${cols}, ${cellW}px)`,
            gridAutoRows: `${cellH}px`,
            gap: GAP,
            justifyContent: "center",
            alignContent: "center",
            width: "100%",
            height: "100%",
          }}
        >
          {Array.from({ length: n }, (_, i) => (
            <CameraTile key={i} index={i} name={CAMERA_URLS[i]?.name} url={CAMERA_URLS[i]?.url} />
          ))}
        </div>
      </div>
    </div>
  );
}