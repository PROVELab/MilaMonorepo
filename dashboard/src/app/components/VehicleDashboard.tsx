"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { DriveMode } from "../types/telemetry";
import { VehicleScene } from "./VehicleScene";
import { Tachometer } from "./hud/Tachometer";
import { DriveModeSelector } from "./hud/DriveModeSelector";
import { CruiseControlPanel } from "./hud/CruiseControlPanel";
import { EmergencyStopButton } from "./hud/EmergencyStopButton";
import { VehicleStatePanel } from "./panels/VehicleStatePanel";
import { ReverseCameraPanel } from "./panels/ReverseCameraPanel";
import { useVehicleTelemetry } from "../hooks/useVehicleTelemetry";

type ViewId = "drive" | "vsr" | "reverse";
const TOAST_LIMIT = 5;
const TOAST_DURATION_MS = 4200;

interface LogToast {
  id: number;
  message: string;
}

interface DriveViewProps {
  batteryPct: number;
  driveMode: DriveMode;
  changeDriveMode: (mode: DriveMode) => void;
  cruiseTargetMph: number | null;
  nudgeCruiseDown: () => void;
  nudgeCruiseUp: () => void;
  engageEmergencyStop: () => void;
  speedMph: number;
  pedalPct: number;
}

function DriveView({
  batteryPct,
  driveMode,
  changeDriveMode,
  cruiseTargetMph,
  nudgeCruiseDown,
  nudgeCruiseUp,
  engageEmergencyStop,
  speedMph,
  pedalPct,
}: DriveViewProps) {
  return (
    <div className="drive-view">
      <VehicleScene speed={speedMph} driveMode={driveMode} />
      <div className="drive-view__overlay drive-view__overlay--top">
        <Tachometer speed={speedMph} pedalPct={pedalPct} />
      </div>
      <div className="drive-view__overlay drive-view__overlay--bottom">
        <DriveModeSelector value={driveMode} onChange={changeDriveMode} />
        <CruiseControlPanel
          active={driveMode === "Cruise Control"}
          targetMph={cruiseTargetMph}
          onMinus={nudgeCruiseDown}
          onPlus={nudgeCruiseUp}
        />
        <div className="cluster__status">
          <span>Battery</span>
          <strong>{batteryPct.toFixed(1)}%</strong>
        </div>
        <EmergencyStopButton onEngage={engageEmergencyStop} />
      </div>
    </div>
  );
}

export function VehicleDashboard() {
  const {
    snapshot,
    driveMode,
    cruiseTargetMph,
    changeDriveMode,
    nudgeCruiseUp,
    nudgeCruiseDown,
    engageEmergencyStop,
  } = useVehicleTelemetry(180);
  const [activeView, setActiveView] = useState<ViewId>("drive");
  const [toasts, setToasts] = useState<LogToast[]>([]);
  const seenLogsRef = useRef<Set<string>>(new Set());
  const logsPrimedRef = useRef(false);
  const toastSeqRef = useRef(0);
  const toastTimeoutsRef = useRef<number[]>([]);

  const enqueueToast = useCallback((message: string) => {
    const id = toastSeqRef.current++;
    setToasts(prev => {
      const next = [...prev, { id, message }];
      return next.length > TOAST_LIMIT ? next.slice(next.length - TOAST_LIMIT) : next;
    });

    const timeout = window.setTimeout(() => {
      setToasts(prev => prev.filter(toast => toast.id !== id));
      toastTimeoutsRef.current = toastTimeoutsRef.current.filter(handle => handle !== timeout);
    }, TOAST_DURATION_MS);
    toastTimeoutsRef.current.push(timeout);
  }, []);

  useEffect(() => {
    const logs = snapshot.liveTextLogs.filter(line => line.trim().length > 0);
    const seenLogs = seenLogsRef.current;

    if (!logsPrimedRef.current) {
      logs.forEach(line => seenLogs.add(line));
      logsPrimedRef.current = true;
      return;
    }

    const newLogs = logs.filter(line => !seenLogs.has(line));
    if (newLogs.length > 0) {
      for (const line of newLogs.reverse()) {
        enqueueToast(line);
      }
    }

    seenLogs.clear();
    logs.forEach(line => seenLogs.add(line));
  }, [snapshot.liveTextLogs, enqueueToast]);

  useEffect(() => {
    return () => {
      for (const handle of toastTimeoutsRef.current) {
        window.clearTimeout(handle);
      }
      toastTimeoutsRef.current = [];
    };
  }, []);

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
            cruiseTargetMph={cruiseTargetMph}
            nudgeCruiseDown={nudgeCruiseDown}
            nudgeCruiseUp={nudgeCruiseUp}
            engageEmergencyStop={engageEmergencyStop}
            speedMph={snapshot.speedMph}
            pedalPct={snapshot.pedalPct}
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
    [
      snapshot,
      driveMode,
      cruiseTargetMph,
      changeDriveMode,
      nudgeCruiseDown,
      nudgeCruiseUp,
      engageEmergencyStop,
    ],
  );

  const active = views.find(view => view.id === activeView) ?? views[0];

  return (
    <div className="dashboard-fullscreen">
      <div className="toast-stack" aria-live="polite" aria-atomic="false">
        {toasts.map(toast => (
          <div key={toast.id} className="toast-stack__item">
            {toast.message}
          </div>
        ))}
      </div>
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
