"use client";

import { useCallback, useEffect, useMemo, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DriveMode, VehicleSnapshot } from "../types/telemetry";
import { CRUISE_STEP_RPM, DEFAULT_CRUISE_TARGET_RPM } from "../constants/vehicle";

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

export function useVehicleTelemetry(pollIntervalMs = 250) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [runtime, setRuntime] = useState<"unknown" | "tauri" | "browser">("unknown");

  useEffect(() => {
    if (typeof window === "undefined") return;
    setRuntime(runtimeIsTauri() ? "tauri" : "browser");
  }, []);

  const fetchSnapshot = useCallback(async () => {
    const payload = await invoke<VehicleSnapshot>("get_vehicle_snapshot");
    setSnapshot(payload);
  }, []);

  useEffect(() => {
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
  }, [runtime, fetchSnapshot, pollIntervalMs]);

  const sendMotorCommand = useCallback(
    async (command: BackendMotorCommandRequest) => {
      if (runtime !== "tauri") return;
      try {
        await invoke("send_motor_command", { command });
      } catch (error) {
        console.error("failed to send motor command", error);
      }
    },
    [runtime],
  );

  const changeDriveMode = useCallback(
    (nextMode: DriveMode) => {
      if (snapshot.driveMode === nextMode) return;
      const command: BackendMotorCommandRequest = { mode: nextMode };
      if (nextMode === "Cruise Control") {
        command.cruiseTargetRpm = DEFAULT_CRUISE_TARGET_RPM;
      }
      void sendMotorCommand(command);
    },
    [sendMotorCommand, snapshot.driveMode],
  );

  const adjustCruiseRpm = useCallback(
    (deltaRpm: number) => {
      if (snapshot.driveMode !== "Cruise Control") return;
      const currentRpm = snapshot.cruiseTargetRpm ?? DEFAULT_CRUISE_TARGET_RPM;
      const nextRpm = Math.max(0, currentRpm + deltaRpm);
      void sendMotorCommand({ mode: "Cruise Control", cruiseTargetRpm: nextRpm });
    },
    [sendMotorCommand, snapshot.cruiseTargetRpm, snapshot.driveMode],
  );

  const engageEmergencyStop = useCallback(async () => {
    if (runtime !== "tauri") return;
    try {
      await invoke("engage_emergency_stop");
    } catch (error) {
      console.error("failed to send emergency stop", error);
    }
  }, [runtime]);

  return useMemo(
    () => ({
      snapshot,
      runtime,
      driveMode: snapshot.driveMode,
      cruiseTargetRpm: snapshot.cruiseTargetRpm ?? null,
      isSerialReady: snapshot.isSerialReady,
      framesReceived: snapshot.framesReceived,
      lastFrameAgeSeconds: snapshot.lastFrameAgeSeconds ?? null,
      changeDriveMode,
      nudgeCruiseUp: () => adjustCruiseRpm(CRUISE_STEP_RPM),
      nudgeCruiseDown: () => adjustCruiseRpm(-CRUISE_STEP_RPM),
      engageEmergencyStop,
    }),
    [adjustCruiseRpm, changeDriveMode, engageEmergencyStop, runtime, snapshot],
  );
}
