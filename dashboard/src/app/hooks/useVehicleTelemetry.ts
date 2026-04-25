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
  driveMode: "P",
  sections: [],
  liveTextLogs: ["waiting for MCU VSR stream"],
};

type BackendVehicleSnapshot = Omit<VehicleSnapshot, "pedalPct">;

const runtimeIsTauri = () =>
  typeof window !== "undefined" &&
  ("__TAURI_INTERNALS__" in window || "__TAURI_IPC__" in window);

function extractNumericVsrValue(
  snapshot: Pick<VehicleSnapshot, "sections">,
  sectionId: string,
  fieldLabel: string,
): number {
  const rawValue = snapshot.sections
    .find(section => section.id === sectionId)
    ?.fields.find(field => field.label === fieldLabel)
    ?.value;

  const parsed = Number.parseFloat(rawValue ?? "");
  return Number.isFinite(parsed) ? parsed : 0;
}

function clampPct(value: number): number {
  return Math.min(100, Math.max(0, value));
}

function extractMotorSpeedRpm(snapshot: Pick<VehicleSnapshot, "sections">): number {
  return extractNumericVsrValue(snapshot, "motor_speed", "Motor Speed");
}

function extractPedalPct(snapshot: Pick<VehicleSnapshot, "sections">): number {
  return clampPct(extractNumericVsrValue(snapshot, "pedal", "Pedal Position"));
}

export function useVehicleTelemetry(pollIntervalMs = 250) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [driveMode, setDriveMode] = useState<DriveMode>("P");
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
    setDriveMode(payload.driveMode);
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
      setDriveMode(nextMode);
      setSnapshot(prev => ({ ...prev, driveMode: nextMode }));

      if (runtime !== "tauri") return;

      try {
        await invoke("set_drive_mode", { mode: nextMode });
      } catch (error) {
        console.error("failed to update drive mode", error);
      }
    },
    [runtime],
  );

  return { snapshot, driveMode, changeDriveMode };
}
