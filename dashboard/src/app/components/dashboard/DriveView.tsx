"use client";

import { useState } from "react";
import type { DriveMode } from "../../types/telemetry";
import { VehicleScene } from "../VehicleScene";
import { Tachometer } from "../hud/Tachometer";
import { DriveModeSelector } from "../hud/DriveModeSelector";
import { CruiseControlPanel } from "../hud/CruiseControlPanel";

interface Props {
  driveMode: DriveMode;
  motorRpm: number | null;
  pedalPct: number | null;
  brakePct: number | null;
  cruiseTargetRpm: number | null;
  onDriveModeChange: (mode: DriveMode) => void;
  onCruiseDecrease10: () => void;
  onCruiseDecrease50: () => void;
  onCruiseIncrease10: () => void;
  onCruiseIncrease50: () => void;
}

function EmptyChartsPanel() {
  return (
    <div className="panel panel--charts" style={{ display: "flex", alignItems: "center", justifyContent: "center", height: "100%", width: "100%", background: "var(--panel)" }}>
      <div style={{ textAlign: "center", color: "var(--muted)" }}>
        <h3>Charts</h3>
        <p>Empty charts (using VSR charting logic later)</p>
      </div>
    </div>
  );
}

type CenterViewId = "scene" | "charts";
const CENTER_VIEWS: CenterViewId[] = ["scene", "charts"];

export function DriveView({
  driveMode,
  motorRpm,
  pedalPct,
  brakePct,
  cruiseTargetRpm,
  onDriveModeChange,
  onCruiseDecrease10,
  onCruiseDecrease50,
  onCruiseIncrease10,
  onCruiseIncrease50,
}: Props) {
  const [activeCenterIndex, setActiveCenterIndex] = useState(0);
  const activeCenter = CENTER_VIEWS[activeCenterIndex];

  const handlePrev = () => setActiveCenterIndex((i) => (i > 0 ? i - 1 : CENTER_VIEWS.length - 1));
  const handleNext = () => setActiveCenterIndex((i) => (i < CENTER_VIEWS.length - 1 ? i + 1 : 0));

  return (
    <div className="drive-view">
      <div className="drive-view__center" style={{ width: "100%", height: "100%", position: "absolute", inset: 0 }}>
        {activeCenter === "scene" ? (
          <VehicleScene rpm={motorRpm} driveMode={driveMode} />
        ) : (
          <EmptyChartsPanel />
        )}
      </div>

      <button className="nav-arrow nav-arrow--left" onClick={handlePrev} aria-label="Previous view">
        &#10094;
      </button>
      <button className="nav-arrow nav-arrow--right" onClick={handleNext} aria-label="Next view">
        &#10095;
      </button>

      <div className="drive-view__topbar">
        <Tachometer rpm={motorRpm} pedalPct={pedalPct} brakePct={brakePct} />
      </div>
      <div className="drive-view__controls">
        <DriveModeSelector value={driveMode} onChange={onDriveModeChange} />
        {driveMode === "Cruise Control" && (
          <CruiseControlPanel
            active={true}
            targetRpm={cruiseTargetRpm}
            onDecrease10={onCruiseDecrease10}
            onDecrease50={onCruiseDecrease50}
            onIncrease10={onCruiseIncrease10}
            onIncrease50={onCruiseIncrease50}
          />
        )}
      </div>
    </div>
  );
}
