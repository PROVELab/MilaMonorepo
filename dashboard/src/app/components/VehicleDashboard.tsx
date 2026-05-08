"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { DriveView } from "./dashboard/DriveView";
import { ToastStack } from "./dashboard/ToastStack";
import { DashboardViewId, ViewNav } from "./dashboard/ViewNav";
import { VehicleStatePanel } from "./panels/VehicleStatePanel";
import { ReverseCameraPanel } from "./panels/ReverseCameraPanel";
import { EmergencyStopButton } from "./hud/EmergencyStopButton";
import { useLogToasts } from "../hooks/useLogToasts";
import { useVehicleTelemetry } from "../hooks/useVehicleTelemetry";
import { LoadingScreen } from "./dashboard/LoadingScreen";

const LOADING_FADE_MS = 420;

export function VehicleDashboard() {
  const {
    snapshot,
    driveMode,
    cruiseTargetRpm,
    isSerialReady,
    framesReceived,
    lastFrameAgeSeconds,
    changeDriveMode,
    nudgeCruiseUp,
    nudgeCruiseDown,
    engageEmergencyStop,
  } = useVehicleTelemetry(180);
  const [activeView, setActiveView] = useState<DashboardViewId>("drive");
  const [showLoading, setShowLoading] = useState(true);
  const [isLoadingFadingOut, setIsLoadingFadingOut] = useState(false);
  const [isSkipped, setIsSkipped] = useState(false);
  const loadingFadeTimerRef = useRef<number | null>(null);
  const toasts = useLogToasts(snapshot.liveTextLogs);

  const beginLoadingFadeOut = useCallback(() => {
    if (!showLoading || isLoadingFadingOut) {
      return;
    }
    setIsLoadingFadingOut(true);
    if (loadingFadeTimerRef.current) {
      window.clearTimeout(loadingFadeTimerRef.current);
    }
    loadingFadeTimerRef.current = window.setTimeout(() => {
      setShowLoading(false);
      setIsLoadingFadingOut(false);
      loadingFadeTimerRef.current = null;
    }, LOADING_FADE_MS);
  }, [showLoading, isLoadingFadingOut]);

  useEffect(() => {
    if (!isSerialReady && !showLoading && !isSkipped) {
      setShowLoading(true);
      setIsLoadingFadingOut(false);
    }
  }, [isSerialReady, isSkipped, showLoading]);

  useEffect(() => {
    if (isSerialReady && isSkipped) {
      setIsSkipped(false);
    }
  }, [isSerialReady, isSkipped]);

  useEffect(
    () => () => {
      if (loadingFadeTimerRef.current) {
        window.clearTimeout(loadingFadeTimerRef.current);
      }
    },
    [],
  );

  const handleSkip = () => {
    setIsSkipped(true);
    beginLoadingFadeOut();
  };

  const loadingReason = useMemo(() => {
    if (framesReceived === 0) {
      return "attempting to connect to MCU";
    }
    const age = Math.max(0, lastFrameAgeSeconds ?? 0);
    const suffix = age === 1 ? "" : "s";
    return `VSR not received in ${age} second${suffix}`;
  }, [framesReceived, lastFrameAgeSeconds]);

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

    document.addEventListener('wheel', handleWheel, { passive: false });
    document.addEventListener('keydown', handleKeyDown, { passive: false });
    document.addEventListener('touchmove', handleTouchMove, { passive: false });

    return () => {
      document.removeEventListener('wheel', handleWheel);
      document.removeEventListener('keydown', handleKeyDown);
      document.removeEventListener('touchmove', handleTouchMove);
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
        brakePct={snapshot.brakePct}
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
    snapshot.brakePct,
    snapshot.sections,
  ]);

  return (
    <div className="dashboard-fullscreen">
      {showLoading && (
        <LoadingScreen
          reason={loadingReason}
          isReady={isSerialReady}
          onSkip={handleSkip}
          isFadingOut={isLoadingFadingOut}
        />
      )}
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
