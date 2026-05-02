"use client";

import { useEffect, useMemo, useState } from "react";
import { DriveView } from "./dashboard/DriveView";
import { ToastStack } from "./dashboard/ToastStack";
import { DashboardViewId, ViewNav } from "./dashboard/ViewNav";
import { VehicleStatePanel } from "./panels/VehicleStatePanel";
import { ReverseCameraPanel } from "./panels/ReverseCameraPanel";
import { EmergencyStopButton } from "./hud/EmergencyStopButton";
import { useLogToasts } from "../hooks/useLogToasts";
import { useVehicleTelemetry } from "../hooks/useVehicleTelemetry";
import { LoadingScreen } from "./dashboard/LoadingScreen";

export function VehicleDashboard() {
  const {
    snapshot,
    driveMode,
    cruiseTargetRpm,
    isSerialReady,
    changeDriveMode,
    nudgeCruiseUp,
    nudgeCruiseDown,
    engageEmergencyStop,
  } = useVehicleTelemetry(180);
  const [activeView, setActiveView] = useState<DashboardViewId>("drive");
  const [showLoading, setShowLoading] = useState(true);
  const [isSkipped, setIsSkipped] = useState(false);
  const toasts = useLogToasts(snapshot.liveTextLogs);

  useEffect(() => {
    // If serial is lost, and we weren't already showing loading, reset to loading.
    // unless the user has manually skipped.
    if (!isSerialReady && !showLoading && !isSkipped) {
      setShowLoading(true);
    }

    if (isSerialReady && showLoading) {
      // Extended timing for high-fidelity startup sequence (4.5s draw + 3s hold/sweep)
      const timer = setTimeout(() => {
        setShowLoading(false);
      }, 7500);
      return () => clearTimeout(timer);
    }
  }, [isSerialReady, showLoading, isSkipped]);

  const handleSkip = () => {
    setIsSkipped(true);
    setShowLoading(false);
  };

  useEffect(() => {
    const handleWheel = (e: WheelEvent) => {
      if (e.ctrlKey) {
        e.preventDefault();
      }
    };
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.ctrlKey && (e.key === '=' || e.key === '-' || e.key === '+')) {
        e.preventDefault();
      }
    };
    const handleTouchMove = (e: TouchEvent) => {
      if (e.touches.length > 1) {
        e.preventDefault();
      }
    };
    const preventGestures = (e: Event) => e.preventDefault();

    document.addEventListener('wheel', handleWheel, { passive: false });
    document.addEventListener('keydown', handleKeyDown, { passive: false });
    document.addEventListener('touchmove', handleTouchMove, { passive: false });
    document.addEventListener('gesturestart', preventGestures, { passive: false } as any);
    document.addEventListener('gesturechange', preventGestures, { passive: false } as any);
    document.addEventListener('gestureend', preventGestures, { passive: false } as any);

    return () => {
      document.removeEventListener('wheel', handleWheel);
      document.removeEventListener('keydown', handleKeyDown);
      document.removeEventListener('touchmove', handleTouchMove);
      document.removeEventListener('gesturestart', preventGestures as any);
      document.removeEventListener('gesturechange', preventGestures as any);
      document.removeEventListener('gestureend', preventGestures as any);
    };
  }, []);

  const activeElement = useMemo(() => {
    if (activeView === "vsr") {
      return <VehicleStatePanel sections={snapshot.sections} logs={snapshot.liveTextLogs} />;
    }
    if (activeView === "reverse") {
      return <ReverseCameraPanel />;
    }
    return (
      <DriveView
        driveMode={driveMode}
        motorRpm={snapshot.motorRpm}
        pedalPct={snapshot.pedalPct}
        cruiseTargetRpm={cruiseTargetRpm}
        onDriveModeChange={changeDriveMode}
        onCruiseDown={nudgeCruiseDown}
        onCruiseUp={nudgeCruiseUp}
      />
    );
  }, [
    activeView,
    changeDriveMode,
    cruiseTargetRpm,
    driveMode,
    nudgeCruiseDown,
    nudgeCruiseUp,
    snapshot.liveTextLogs,
    snapshot.motorRpm,
    snapshot.pedalPct,
    snapshot.sections,
  ]);

  return (
    <div className="dashboard-fullscreen">
      {showLoading && <LoadingScreen onSkip={handleSkip} />}
      <ToastStack toasts={toasts} />
      <main className="view-stage">
        <div key={activeView} className="view-stage__scene">
          {activeElement}
        </div>
      </main>
      <div className="global-bottom-bar">
        <ViewNav activeView={activeView} onChange={setActiveView} />
        <div className="global-bottom-bar__estop">
          <EmergencyStopButton onEngage={engageEmergencyStop} />
        </div>
      </div>
    </div>
  );
}
