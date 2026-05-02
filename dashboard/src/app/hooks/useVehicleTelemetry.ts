"use client";

import { useCallback, useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DriveMode, VehicleSnapshot } from "../types/telemetry";
import { MOTOR_SPEED_RPM_TO_MPH } from "../constants/vehicle";

declare global {
  interface Window {
    __TAURI_IPC__?: unknown;
    __TAURI_INTERNALS__?: unknown;
  }
}

const DEFAULT_SNAPSHOT: VehicleSnapshot = {
  speedMph: 0,
  pedalPct: 0,
  torqueRatio: 0,
  batteryPct: 0,
  driveMode: "Park",
  cruiseTargetRpm: null,
  sections: [],
  liveTextLogs: ["waiting for MCU VSR stream"],
};

type BackendVehicleSnapshot = Omit<VehicleSnapshot, "pedalPct">;
type BackendMotorCommandRequest = {
  mode: DriveMode;
  cruiseTargetRpm?: number;
};

const runtimeIsTauri = () =>
  typeof window !== "undefined" &&
  ("__TAURI_INTERNALS__" in window || "__TAURI_IPC__" in window);

const DEFAULT_CRUISE_TARGET_MPH = 10;
const CRUISE_STEP_MPH = 5;

function extractNumericVsrValue(
  snapshot: Pick<VehicleSnapshot, "sections">,
  sectionId: string,
  fieldKey: string,
): number {
  const rawValue = snapshot.sections
    .find(section => section.id === sectionId)
    ?.fields.find(field => field.key === fieldKey)
    ?.value;

  const parsed = Number.parseFloat(rawValue ?? "");
  return Number.isFinite(parsed) ? parsed : 0;
}

function clampPct(value: number): number {
  return Math.min(100, Math.max(0, value));
}

function extractMotorSpeedRpm(snapshot: Pick<VehicleSnapshot, "sections">): number {
  return extractNumericVsrValue(snapshot, "motor_speed", "motor_speed");
}

function extractPedalPct(snapshot: Pick<VehicleSnapshot, "sections">): number {
  return clampPct(extractNumericVsrValue(snapshot, "pedal", "pedal_position_pct"));
}

function mphToRpm(speedMph: number): number {
  return Math.max(0, Math.round(speedMph / MOTOR_SPEED_RPM_TO_MPH));
}

function rpmToMph(speedRpm: number): number {
  return speedRpm * MOTOR_SPEED_RPM_TO_MPH;
}

export function useVehicleTelemetry(pollIntervalMs = 250) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [runtime, setRuntime] = useState<"unknown" | "tauri" | "sim">("unknown");

  useEffect(() => {
    if (typeof window === "undefined") return;
    setRuntime(runtimeIsTauri() ? "tauri" : "sim");
  }, []);

  const fetchSnapshot = useCallback(async () => {
    const payload = await invoke<BackendVehicleSnapshot>("get_vehicle_snapshot");
    const motorSpeedRpm = extractMotorSpeedRpm(payload);
    const pedalPct = extractPedalPct(payload);
    setSnapshot({
      ...payload,
      speedMph: motorSpeedRpm * MOTOR_SPEED_RPM_TO_MPH,
      pedalPct,
    });
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

      if (stopped) return;
      timer = window.setTimeout(tick, pollIntervalMs);
    };

    tick();
    return () => {
      stopped = true;
      if (timer) window.clearTimeout(timer);
    };
  }, [runtime, fetchSnapshot, pollIntervalMs]);

  const changeDriveMode = useCallback(
    async (nextMode: DriveMode) => {
      if (runtime !== "tauri") return;
      if (snapshot.driveMode === nextMode) return;

      const command: BackendMotorCommandRequest = { mode: nextMode };
      if (nextMode === "Cruise Control") {
        command.cruiseTargetRpm = mphToRpm(DEFAULT_CRUISE_TARGET_MPH);
      }

      try {
        await invoke("send_motor_command", { command });
      } catch (error) {
        console.error("failed to send drive mode command", error);
      }
    },
    [runtime, snapshot.driveMode],
  );

  const bumpCruiseTarget = useCallback(
    async (deltaMph: number) => {
      if (runtime !== "tauri" || snapshot.driveMode !== "Cruise Control") return;

      const currentMph = rpmToMph(snapshot.cruiseTargetRpm ?? mphToRpm(DEFAULT_CRUISE_TARGET_MPH));
      const nextMph = Math.max(0, currentMph + deltaMph);

      try {
        await invoke("send_motor_command", {
          command: {
            mode: "Cruise Control",
            cruiseTargetRpm: mphToRpm(nextMph),
          } satisfies BackendMotorCommandRequest,
        });
      } catch (error) {
        console.error("failed to update cruise-control target", error);
      }
    },
    [runtime, snapshot.driveMode, snapshot.cruiseTargetRpm],
  );

  const engageEmergencyStop = useCallback(async () => {
    if (runtime !== "tauri") return;

    try {
      await invoke("engage_emergency_stop");
    } catch (error) {
      console.error("failed to send emergency stop", error);
    }
  }, [runtime]);

  const cruiseTargetMph =
    snapshot.cruiseTargetRpm == null ? null : rpmToMph(snapshot.cruiseTargetRpm);

  const nudgeCruiseUp = useCallback(() => {
    void bumpCruiseTarget(CRUISE_STEP_MPH);
  }, [bumpCruiseTarget]);

  const nudgeCruiseDown = useCallback(() => {
    void bumpCruiseTarget(-CRUISE_STEP_MPH);
  }, [bumpCruiseTarget]);

  return {
    snapshot,
    driveMode: snapshot.driveMode,
    cruiseTargetMph,
    changeDriveMode,
    nudgeCruiseUp,
    nudgeCruiseDown,
    engageEmergencyStop,
  };
}
