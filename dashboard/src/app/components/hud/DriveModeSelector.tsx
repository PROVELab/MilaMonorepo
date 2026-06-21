"use client";

import { useMemo, useRef } from "react";
import type { DriveMode } from "../../types/telemetry";

interface Props {
  value: DriveMode;
  onChange?: (mode: DriveMode) => void;
}

type DragState = {
  pointerId: number;
  startY: number;
  startedOnButton: boolean;
  didDrag: boolean;
};

export function DriveModeSelector({ value, onChange }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const dragRef = useRef<DragState | null>(null);
  const suppressClickRef = useRef(false);
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
    const target = event.target as HTMLElement;
    dragRef.current = {
      pointerId: event.pointerId,
      startY: event.clientY,
      startedOnButton: target.closest("button") !== null,
      didDrag: false,
    };
  };

  const handlePointerMove = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!dragRef.current || dragRef.current.pointerId !== event.pointerId) return;

    const dragDistance = event.clientY - dragRef.current.startY;
    const threshold = 28;
    if (Math.abs(dragDistance) <= threshold || dragRef.current.didDrag) return;

    dragRef.current.didDrag = true;
    suppressClickRef.current = true;

    if (containerRef.current && !containerRef.current.hasPointerCapture(event.pointerId)) {
      containerRef.current.setPointerCapture(event.pointerId);
    }
  };

  const handlePointerUp = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!dragRef.current || dragRef.current.pointerId !== event.pointerId) return;

    if (containerRef.current?.hasPointerCapture(event.pointerId)) {
      containerRef.current.releasePointerCapture(event.pointerId);
    }

    const dragDistance = event.clientY - dragRef.current.startY;
    const didDrag = dragRef.current.didDrag;
    const startedOnButton = dragRef.current.startedOnButton;
    const threshold = 28;

    if (dragDistance > threshold) {
      commitChange(activeIndex + 1);
    } else if (dragDistance < -threshold) {
      commitChange(activeIndex - 1);
    } else if (!startedOnButton && didDrag) {
      suppressClickRef.current = false;
    }

    dragRef.current = null;
  };

  const handlePointerCancel = (event: React.PointerEvent<HTMLDivElement>) => {
    if (!dragRef.current || dragRef.current.pointerId !== event.pointerId) return;

    if (containerRef.current?.hasPointerCapture(event.pointerId)) {
      containerRef.current.releasePointerCapture(event.pointerId);
    }

    dragRef.current = null;
    suppressClickRef.current = false;
  };

  const handleModeClick = (modeIndex: number) => {
    if (suppressClickRef.current) {
      suppressClickRef.current = false;
      return;
    }
    commitChange(modeIndex);
  };

  return (
    <div
      ref={containerRef}
      className="drive-selector"
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerCancel}
    >
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
                onClick={() => handleModeClick(modes.indexOf(mode))}
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
