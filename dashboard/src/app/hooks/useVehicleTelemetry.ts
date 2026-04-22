"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DriveMode, VehicleSnapshot } from "../types/telemetry";

declare global {
  interface Window {
    __TAURI_IPC__?: unknown;
    __TAURI_INTERNALS__?: unknown;
  }
}

const DEFAULT_SNAPSHOT: VehicleSnapshot = {
  speedMph: 0,
  torqueRatio: 0,
  batteryPct: 0,
  driveMode: "P",
  sections: [],
  liveTextLogs: ["waiting for MCU VSR stream"],
};

const runtimeIsTauri = () =>
  typeof window !== "undefined" &&
  ("__TAURI_INTERNALS__" in window || "__TAURI_IPC__" in window);

export function useVehicleTelemetry(pollIntervalMs = 250) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [driveMode, setDriveMode] = useState<DriveMode>("P");
  const [runtime, setRuntime] = useState<"unknown" | "tauri" | "sim">("unknown");

  const modeRef = useRef<DriveMode>("P");

  useEffect(() => {
    if (typeof window === "undefined") return;
    setRuntime(runtimeIsTauri() ? "tauri" : "sim");
  }, []);

  const fetchSnapshot = useCallback(async () => {
    const payload = await invoke<VehicleSnapshot>("get_vehicle_snapshot");
    setSnapshot(payload);
    setDriveMode(payload.driveMode);
    modeRef.current = payload.driveMode;
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
      modeRef.current = nextMode;
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
