"use client";

import { useMemo, useRef } from "react";
import type { DriveMode } from "../../types/telemetry";

interface Props {
  value: DriveMode;
  onChange?: (mode: DriveMode) => void;
}

export function DriveModeSelector({ value, onChange }: Props) {
  const dragRef = useRef<{ startY: number } | null>(null);
  const modes: DriveMode[] = useMemo(() => ["R", "P", "D"], []);
  const activeIndex = modes.indexOf(value);

  const commitChange = (nextIndex: number) => {
    if (!onChange) return;
    const normalizedIndex = Math.max(0, Math.min(modes.length - 1, nextIndex));
    onChange(modes[normalizedIndex]);
  };

  const handlePointerDown = (event: React.PointerEvent<HTMLDivElement>) => {
    dragRef.current = { startY: event.clientY };
    event.currentTarget.setPointerCapture(event.pointerId);
  };

  const handlePointerUp = (event: React.PointerEvent<HTMLDivElement>) => {
    event.currentTarget.releasePointerCapture(event.pointerId);
    if (!dragRef.current) return;

    const dragDistance = event.clientY - dragRef.current.startY;
    const threshold = 28;

    if (dragDistance > threshold) {
      commitChange(activeIndex + 1);
    } else if (dragDistance < -threshold) {
      commitChange(activeIndex - 1);
    }

    dragRef.current = null;
  };

  return (
    <div className="drive-selector" onPointerDown={handlePointerDown} onPointerUp={handlePointerUp}>
      <div className="drive-selector__indicator" style={{ transform: `translateY(${activeIndex * 60}px)` }} />
      {modes.map(mode => (
        <button
          key={mode}
          type="button"
          className={`drive-selector__mode ${mode === value ? "drive-selector__mode--active" : ""}`}
          onClick={() => commitChange(modes.indexOf(mode))}
        >
          {mode}
        </button>
      ))}
    </div>
  );
}
