"use client";

import { useMemo, useState } from "react";
import type { DriveMode } from "../types/telemetry";
import { VehicleScene } from "./VehicleScene";
import { Tachometer } from "./hud/Tachometer";
import { DriveModeSelector } from "./hud/DriveModeSelector";
import { EmergencyStopButton } from "./hud/EmergencyStopButton";
import { VehicleStatePanel } from "./panels/VehicleStatePanel";
import { ReverseCameraPanel } from "./panels/ReverseCameraPanel";
import { useVehicleTelemetry } from "../hooks/useVehicleTelemetry";

type ViewId = "drive" | "vsr" | "reverse";

interface DriveViewProps {
  batteryPct: number;
  driveMode: DriveMode;
  changeDriveMode: (mode: DriveMode) => void;
  speedMph: number;
  torqueRatio: number;
}

function DriveView({ batteryPct, driveMode, changeDriveMode, speedMph, torqueRatio }: DriveViewProps) {
  return (
    <div className="drive-view">
      <VehicleScene speed={speedMph} driveMode={driveMode} />
      <div className="drive-view__overlay drive-view__overlay--top">
        <Tachometer speed={speedMph} torqueRatio={torqueRatio} />
      </div>
      <div className="drive-view__overlay drive-view__overlay--bottom">
        <DriveModeSelector value={driveMode} onChange={changeDriveMode} />
        <div className="cluster__status">
          <span>Battery</span>
          <strong>{batteryPct.toFixed(1)}%</strong>
        </div>
        <EmergencyStopButton />
      </div>
    </div>
  );
}

export function VehicleDashboard() {
  const { snapshot, driveMode, changeDriveMode } = useVehicleTelemetry(180);
  const [activeView, setActiveView] = useState<ViewId>("drive");

  const views = useMemo(
    () => [
      {
        id: "drive" as const,
        label: "Drive",
        element: (
          <DriveView
            batteryPct={snapshot.batteryPct}
            driveMode={driveMode}
            changeDriveMode={changeDriveMode}
            speedMph={snapshot.speedMph}
            torqueRatio={snapshot.torqueRatio}
          />
        ),
      },
      {
        id: "vsr" as const,
        label: "VSR + Logs",
        element: <VehicleStatePanel sections={snapshot.sections} logs={snapshot.liveTextLogs} />,
      },
      { id: "reverse" as const, label: "Reverse Camera", element: <ReverseCameraPanel /> },
    ],
    [snapshot, driveMode, changeDriveMode],
  );

  const active = views.find(view => view.id === activeView) ?? views[0];

  return (
    <div className="dashboard-fullscreen">
      <div className="view-stage">
        <div key={active.id} className="view-stage__scene">
          {active.element}
        </div>
      </div>
      <nav className="view-nav">
        {views.map(view => (
          <button
            key={view.id}
            type="button"
            className={view.id === activeView ? "is-active" : ""}
            onClick={() => setActiveView(view.id)}
          >
            {view.label}
          </button>
        ))}
      </nav>
    </div>
  );
}
