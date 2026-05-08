"use client";

import { useMemo, useRef } from "react";
import type { DriveMode } from "../../types/telemetry";

interface Props {
  value: DriveMode;
  onChange?: (mode: DriveMode) => void;
}

export function DriveModeSelector({ value, onChange }: Props) {
  const dragRef = useRef<{ startY: number } | null>(null);
  const modes = useMemo(
    () =>
      [
        { value: "Reverse" as DriveMode, label: "R", name: "Reverse" },
        { value: "Park" as DriveMode, label: "P", name: "Park" },
        { value: "Neutral" as DriveMode, label: "N", name: "Neutral" },
        { value: "Drive" as DriveMode, label: "D", name: "Drive" },
        { value: "Cruise Control" as DriveMode, label: "C", name: "Cruise" },
      ] satisfies ReadonlyArray<{ value: DriveMode; label: string; name: string }>,
    [],
  );
  const rows = useMemo(
    () => [[modes[0]], [modes[1], modes[2]], [modes[3]], [modes[4]]],
    [modes],
  );
  const activeIndex = Math.max(0, modes.findIndex(mode => mode.value === value));

  const commitChange = (nextIndex: number) => {
    if (!onChange) return;
    const normalizedIndex = Math.max(0, Math.min(modes.length - 1, nextIndex));
    onChange(modes[normalizedIndex].value);
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
      <div className="drive-selector__header">Drive Mode</div>
      <div className="drive-selector__grid">
        {rows.map((row, rowIndex) => (
          <div
            key={rowIndex}
            className={`drive-selector__row ${row.length > 1 ? "drive-selector__row--dual" : ""}`}
          >
            {row.map(mode => (
              <button
                key={mode.value}
                type="button"
                className={`drive-selector__mode ${mode.value === value ? "drive-selector__mode--active" : ""}`}
                onClick={() => commitChange(modes.indexOf(mode))}
              >
                <span>{mode.label}</span>
                <em>{mode.name}</em>
              </button>
            ))}
          </div>
        ))}
      </div>
      <div className="drive-selector__hint">
        {modes[activeIndex]?.name}
      </div>
    </div>
  );
}
