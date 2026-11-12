"use client";

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DriveMode, VehicleSnapshot } from "../types/telemetry";
import { CRUISE_COARSE_STEP_RPM, CRUISE_FINE_STEP_RPM } from "../constants/vehicle";

declare global {
  interface Window {
    __TAURI_IPC__?: unknown;
    __TAURI_INTERNALS__?: unknown;
  }
}

const DEFAULT_SNAPSHOT: VehicleSnapshot = {
  motorRpm: null,
  pedalPct: null,
  brakePct: null,
  driveMode: "Park",
  cruiseTargetRpm: null,
  sections: [],
  liveTextLogs: ["waiting for MCU VSR stream"],
  isSerialReady: false,
  framesReceived: 0,
  lastFrameAgeSeconds: null,
};

type BackendMotorCommandRequest = {
  mode: DriveMode;
  cruiseTargetRpm?: number;
};

function runtimeIsTauri() {
  return (
    typeof window !== "undefined" &&
    ("__TAURI_INTERNALS__" in window || "__TAURI_IPC__" in window)
  );
}

function toCruiseTargetRpm(rpm: number | null | undefined) {
  if (rpm == null || !Number.isFinite(rpm)) return 0;
  return Math.round(Math.abs(rpm));
}

export function useVehicleTelemetry(pollIntervalMs = 100) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [runtime, setRuntime] = useState<"unknown" | "tauri" | "browser">("unknown");
  const [isLocalDemo, setIsLocalDemo] = useState(false);
  const [demoSteering, setDemoSteering] = useState<number | null>(null);
  const demoKeysRef = useRef(new Set<string>());
  const demoRpmRef = useRef(0);

  useEffect(() => {
    if (typeof window === "undefined") return;
    setRuntime(runtimeIsTauri() ? "tauri" : "browser");
  }, []);

  const fetchSnapshot = useCallback(async () => {
    const payload = await invoke<VehicleSnapshot>("get_vehicle_snapshot");
    setSnapshot(payload);
  }, []);

  useEffect(() => {
    if (isLocalDemo) return;

    if (runtime !== "tauri") {
      setSnapshot(prev => ({
        ...prev,
        liveTextLogs: ["non-tauri runtime: backend serial reader unavailable"],
      }));
      return;
    }

    let stopped = false;
    let timer: number | null = null;

    const tick = async () => {
      if (stopped) return;
      try {
        await fetchSnapshot();
      } catch (error) {
        console.warn("failed to fetch telemetry snapshot", error);
      }

      if (!stopped) {
        timer = window.setTimeout(tick, pollIntervalMs);
      }
    };

    void tick();
    return () => {
      stopped = true;
      if (timer) window.clearTimeout(timer);
    };
  }, [runtime, isLocalDemo, fetchSnapshot, pollIntervalMs]);

  useEffect(() => {
    if (!isLocalDemo) return;

    const onKeyChange = (event: KeyboardEvent, pressed: boolean) => {
      if (!["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", " "].includes(event.key)) return;
      event.preventDefault();
      if (pressed) demoKeysRef.current.add(event.key);
      else demoKeysRef.current.delete(event.key);
    };
    const onKeyDown = (event: KeyboardEvent) => onKeyChange(event, true);
    const onKeyUp = (event: KeyboardEvent) => onKeyChange(event, false);
    const onBlur = () => demoKeysRef.current.clear();
    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);
    window.addEventListener("blur", onBlur);

    const timer = window.setInterval(() => {
      const keys = demoKeysRef.current;
      const throttle = keys.has("ArrowUp") ? 1 : keys.has("ArrowDown") ? -1 : 0;
      const brake = keys.has(" ");
      demoRpmRef.current += throttle * 90;
      demoRpmRef.current *= brake ? 0.7 : 0.985;
      demoRpmRef.current = Math.max(-3000, Math.min(3000, demoRpmRef.current));
      if (Math.abs(demoRpmRef.current) < 2) demoRpmRef.current = 0;

      const steering = (keys.has("ArrowRight") ? 1 : 0) - (keys.has("ArrowLeft") ? 1 : 0);
      setDemoSteering(steering * 24);
      setSnapshot(prev => ({
        ...prev,
        motorRpm: demoRpmRef.current,
        pedalPct: Math.abs(throttle) * 100,
        brakePct: brake ? 100 : 0,
        driveMode: demoRpmRef.current < -2 ? "Reverse" : demoRpmRef.current > 2 ? "Drive" : "Park",
      }));
    }, 50);

    return () => {
      window.clearInterval(timer);
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
      window.removeEventListener("blur", onBlur);
      demoKeysRef.current.clear();
    };
  }, [isLocalDemo]);

  const setLocalDemoEnabled = useCallback((enabled: boolean) => {
    demoKeysRef.current.clear();
    demoRpmRef.current = 0;
    setDemoSteering(enabled ? 0 : null);
    setIsLocalDemo(enabled);
    setSnapshot(enabled ? {
      ...DEFAULT_SNAPSHOT,
      motorRpm: 0,
      pedalPct: 0,
      brakePct: 0,
      liveTextLogs: ["DashboardLocalDemo active — Arrow keys drive, Space brakes"],
    } : DEFAULT_SNAPSHOT);
  }, []);

  const sendMotorCommand = useCallback(
    async (command: BackendMotorCommandRequest) => {
      if (runtime !== "tauri" || isLocalDemo) return;
      try {
        await invoke("send_motor_command", { command });
      } catch (error) {
        console.error("failed to send motor command", error);
      }
    },
    [runtime, isLocalDemo, snapshot.driveMode, snapshot.isSerialReady],
  );

  const changeDriveMode = useCallback(
    (nextMode: DriveMode) => {
      if (snapshot.driveMode === nextMode) return;
      const command: BackendMotorCommandRequest = { mode: nextMode };
      if (nextMode === "Cruise Control") {
        command.cruiseTargetRpm = toCruiseTargetRpm(snapshot.motorRpm);
      }
      void sendMotorCommand(command);
    },
    [sendMotorCommand, snapshot.driveMode, snapshot.isSerialReady, snapshot.motorRpm],
  );

  const adjustCruiseRpm = useCallback(
    (deltaRpm: number) => {
      if (snapshot.driveMode !== "Cruise Control") return;
      const currentRpm = snapshot.cruiseTargetRpm ?? toCruiseTargetRpm(snapshot.motorRpm);
      const nextRpm = Math.max(0, currentRpm + deltaRpm);
      void sendMotorCommand({ mode: "Cruise Control", cruiseTargetRpm: nextRpm });
    },
    [sendMotorCommand, snapshot.cruiseTargetRpm, snapshot.driveMode, snapshot.motorRpm],
  );

  const engageEmergencyStop = useCallback(async () => {
    if (isLocalDemo) {
      demoRpmRef.current = 0;
      setSnapshot(prev => ({ ...prev, motorRpm: 0, pedalPct: 0, brakePct: 100, driveMode: "Park" }));
      return;
    }
    if (runtime !== "tauri") return;
    try {
      await invoke("engage_emergency_stop");
    } catch (error) {
      console.error("failed to send emergency stop", error);
    }
  }, [runtime, isLocalDemo]);

  return useMemo(
    () => ({
      snapshot,
      runtime,
      isLocalDemo,
      setLocalDemoEnabled,
      demoSteering,
      driveMode: snapshot.driveMode,
      cruiseTargetRpm: snapshot.cruiseTargetRpm ?? null,
      isSerialReady: snapshot.isSerialReady,
      framesReceived: snapshot.framesReceived,
      lastFrameAgeSeconds: snapshot.lastFrameAgeSeconds ?? null,
      changeDriveMode,
      decreaseCruiseBy10: () => adjustCruiseRpm(-CRUISE_FINE_STEP_RPM),
      decreaseCruiseBy50: () => adjustCruiseRpm(-CRUISE_COARSE_STEP_RPM),
      increaseCruiseBy10: () => adjustCruiseRpm(CRUISE_FINE_STEP_RPM),
      increaseCruiseBy50: () => adjustCruiseRpm(CRUISE_COARSE_STEP_RPM),
      engageEmergencyStop,
    }),
    [adjustCruiseRpm, changeDriveMode, demoSteering, engageEmergencyStop, isLocalDemo, runtime, setLocalDemoEnabled, snapshot],
  );
}
