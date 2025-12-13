"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DriveMode, VehicleSnapshot } from "../types/telemetry";

declare global {
  interface Window {
    __TAURI_IPC__?: unknown;
  }
}

const DEFAULT_SNAPSHOT: VehicleSnapshot = {
  speedMph: 0,
  torqueRatio: 0,
  batteryPct: 82,
  driveMode: "P",
  imu: { pitch: 0, roll: 0, yaw: 0 },
  sections: [],
  liveTextLogs: ["telemetry buffer warming up"],
  derivedMetrics: [],
};

const runtimeIsTauri = () => typeof window !== "undefined" && "__TAURI_IPC__" in window;

export function useVehicleTelemetry(pollIntervalMs = 250) {
  const [snapshot, setSnapshot] = useState<VehicleSnapshot>(DEFAULT_SNAPSHOT);
  const [driveMode, setDriveMode] = useState<DriveMode>("P");
  const [runtime, setRuntime] = useState<"unknown" | "tauri" | "sim">("unknown");

  const modeRef = useRef<DriveMode>("P");
  const fallbackLogs = useRef<string[]>(DEFAULT_SNAPSHOT.liveTextLogs);

  useEffect(() => {
    if (typeof window === "undefined") return;
    setRuntime(runtimeIsTauri() ? "tauri" : "sim");
  }, []);

  const buildSections = useCallback(
    (mode: DriveMode, speed: number, torque: number, battery: number, timeSeconds: number) => {
      const packVoltage = 780 + Math.sin(timeSeconds * 0.25) * 14;
      const packCurrent = torque * 420;
      const coolantIn = 32 + Math.sin(timeSeconds * 0.3) * 3;
      const coolantOut = coolantIn + 5 + Math.sin(timeSeconds * 0.6);
      const moduleDelta = 1.5 + Math.sin(timeSeconds * 0.2);
      const hvIsolation = 4.8 + Math.sin(timeSeconds * 0.5) * 0.2;
      const inverterTemp = 48 + Math.sin(timeSeconds * 0.5) * 6;
      const gearboxTemp = 80 + Math.sin(timeSeconds * 0.25) * 4;
      const suspension = 110 + Math.sin(timeSeconds * 0.9) * 5;

      return [
        {
          id: "battery-pack",
          title: "Battery Pack",
          fields: [
            { label: "State of Charge", value: battery.toFixed(1), unit: "%" },
            { label: "Pack Voltage", value: packVoltage.toFixed(0), unit: "V" },
            { label: "Pack Current", value: packCurrent.toFixed(0), unit: "A" },
            { label: "Module Δ", value: moduleDelta.toFixed(2), unit: "°C" },
            { label: "Coolant Inlet", value: coolantIn.toFixed(1), unit: "°C" },
            { label: "Coolant Outlet", value: coolantOut.toFixed(1), unit: "°C" },
            { label: "Heater Duty", value: (Math.sin(timeSeconds * 0.4) * 15 + 20).toFixed(0), unit: "%" },
            { label: "Isolation", value: hvIsolation.toFixed(2), unit: "kΩ" },
            { label: "Contactor State", value: torque > 0.05 ? "Closed" : "Open" },
            { label: "Charge Rate", value: (Math.max(0, Math.sin(timeSeconds * 0.3)) * 150).toFixed(0), unit: "kW" },
          ],
        },
        {
          id: "powertrain",
          title: "Powertrain",
          fields: [
            { label: "Vehicle Speed", value: speed.toFixed(1), unit: "mph" },
            { label: "Front Torque", value: (torque * 320).toFixed(0), unit: "Nm" },
            { label: "Rear Torque", value: (torque * 360).toFixed(0), unit: "Nm" },
            { label: "Inverter Temp", value: inverterTemp.toFixed(1), unit: "°C" },
            { label: "DC/DC Load", value: (Math.sin(timeSeconds * 0.2) * 0.8 + 1.6).toFixed(2), unit: "kW" },
            { label: "Regen Limit", value: (torque * 210).toFixed(0), unit: "Nm" },
            { label: "Gearbox Temp", value: gearboxTemp.toFixed(1), unit: "°C" },
            { label: "Clutch Slip", value: (Math.abs(Math.sin(timeSeconds * 0.8)) * 1.8).toFixed(2), unit: "%" },
            { label: "Drive Mode", value: mode },
          ],
        },
        {
          id: "chassis",
          title: "Chassis & Dynamics",
          fields: [
            { label: "Yaw Rate", value: (Math.sin(timeSeconds * 0.9) * 6).toFixed(2), unit: "°/s" },
            { label: "Pitch Rate", value: (Math.cos(timeSeconds * 0.7) * 3).toFixed(2), unit: "°/s" },
            { label: "Roll Rate", value: (Math.sin(timeSeconds * 0.4) * 2).toFixed(2), unit: "°/s" },
            { label: "Suspension FL", value: (suspension + Math.sin(timeSeconds)).toFixed(0), unit: "mm" },
            { label: "Suspension FR", value: (suspension + Math.cos(timeSeconds)).toFixed(0), unit: "mm" },
            { label: "Suspension RL", value: (suspension + Math.sin(timeSeconds * 1.3)).toFixed(0), unit: "mm" },
            { label: "Suspension RR", value: (suspension + Math.cos(timeSeconds * 1.1)).toFixed(0), unit: "mm" },
            { label: "Brake Temp", value: (220 + Math.abs(Math.sin(timeSeconds)) * 40).toFixed(0), unit: "°C" },
            { label: "Steering Angle", value: (Math.sin(timeSeconds * 0.6) * 14).toFixed(1), unit: "°" },
          ],
        },
        {
          id: "thermal",
          title: "Thermal Systems",
          fields: [
            { label: "Cabin Setpoint", value: (20 + Math.sin(timeSeconds * 0.1)).toFixed(1), unit: "°C" },
            { label: "Cabin Temp", value: (19.5 + Math.sin(timeSeconds * 0.05)).toFixed(1), unit: "°C" },
            { label: "Heat Pump Mode", value: torque > 0.2 ? "Cooling" : "Heating" },
            { label: "Radiator Fan", value: (Math.sin(timeSeconds * 0.9) * 30 + 60).toFixed(0), unit: "%" },
            { label: "Battery Heater", value: battery < 65 ? "On" : "Off" },
            { label: "Compressor Speed", value: (Math.abs(Math.sin(timeSeconds * 0.7)) * 7000).toFixed(0), unit: "rpm" },
            { label: "Ambient", value: (12 + Math.sin(timeSeconds * 0.02) * 6).toFixed(1), unit: "°C" },
          ],
        },
      ];
    },
    [],
  );

  const buildDerivedMetrics = useCallback((timeSeconds: number, torque: number, battery: number) => {
    const makeSparkline = (base: number, variance: number) =>
      Array.from({ length: 30 }, (_, idx) =>
        Number((base + Math.sin(timeSeconds * 0.3 + idx * 0.25) * variance).toFixed(2)),
      );

    const averageSlice = (values: number[], count: number) => {
      const slice = values.slice(-count);
      if (!slice.length) return 0;
      return slice.reduce((sum, value) => sum + value, 0) / slice.length;
    };

    const energySpark = makeSparkline(26 + torque * 8, 2.4);
    const regenSpark = makeSparkline(68 + torque * 20, 4);
    const thermalSpark = makeSparkline(12 + Math.sin(timeSeconds * 0.1) * 2, 1.2);
    const efficiencySpark = makeSparkline(3.6 - torque, 0.2);

    const windows = (spark: number[], unitSuffix = "") => [
      { label: "5m", value: `${averageSlice(spark, 5).toFixed(1)}${unitSuffix}` },
      { label: "10m", value: `${averageSlice(spark, 10).toFixed(1)}${unitSuffix}` },
      { label: "30m", value: `${averageSlice(spark, 20).toFixed(1)}${unitSuffix}` },
    ];

    return [
      {
        label: "Energy Consumption",
        unit: "kWh / 100 mi",
        current: (energySpark.at(-1) ?? 0).toFixed(1),
        windows: windows(energySpark),
        sparkline: energySpark,
      },
      {
        label: "Regen Efficiency",
        unit: "%",
        current: (regenSpark.at(-1) ?? 0).toFixed(1),
        windows: windows(regenSpark, "%"),
        sparkline: regenSpark,
      },
      {
        label: "Thermal Load",
        unit: "kW",
        current: (thermalSpark.at(-1) ?? 0).toFixed(1),
        windows: windows(thermalSpark),
        sparkline: thermalSpark,
      },
      {
        label: "Trip Efficiency",
        unit: "mi / kWh",
        current: Math.max(2.4, efficiencySpark.at(-1) ?? 3).toFixed(2),
        windows: windows(efficiencySpark),
        sparkline: efficiencySpark,
      },
    ];
  }, []);

  const buildFallbackSnapshot = useCallback(
    (mode: DriveMode, timeSeconds: number): VehicleSnapshot => {
      const signedSpeed =
        mode === "D"
          ? 32 + Math.sin(timeSeconds) * 8
          : mode === "R"
          ? -5 + Math.sin(timeSeconds * 1.3)
          : Math.sin(timeSeconds * 0.2) * 0.5;

      const torque =
        mode === "P"
          ? 0
          : 0.35 + 0.25 * Math.abs(Math.sin(timeSeconds * (mode === "R" ? 0.6 : 0.8)));
      const imuPitch = Math.sin(timeSeconds * 0.45) * 9;
      const imuRoll = Math.cos(timeSeconds * 0.35) * 7;
      const imuYaw = (timeSeconds * 22) % 360;
      const battery = 78 + Math.sin(timeSeconds * 0.04) * 3;

      const logEntry = `sim:${new Date().toLocaleTimeString()} speed=${signedSpeed.toFixed(1)} mode=${mode}`;
      fallbackLogs.current = [logEntry, ...fallbackLogs.current].slice(0, 80);

      return {
        speedMph: signedSpeed,
        torqueRatio: torque,
        batteryPct: battery,
        driveMode: mode,
        imu: { pitch: imuPitch, roll: imuRoll, yaw: imuYaw },
        sections: buildSections(mode, signedSpeed, torque, battery, timeSeconds),
        liveTextLogs: fallbackLogs.current,
        derivedMetrics: buildDerivedMetrics(timeSeconds, torque, battery),
      };
    },
    [buildSections, buildDerivedMetrics],
  );

  useEffect(() => {
    if (runtime === "tauri") return;

    let rafId = 0;
    let start: number | null = null;

    const step = (now: number) => {
      if (start === null) start = now;
      const t = (now - start) / 1000;
      setSnapshot(buildFallbackSnapshot(modeRef.current, t));
      rafId = requestAnimationFrame(step);
    };

    rafId = requestAnimationFrame(step);
    return () => cancelAnimationFrame(rafId);
  }, [runtime, buildFallbackSnapshot]);

  const fetchSnapshot = useCallback(async () => {
    try {
      const payload = await invoke<VehicleSnapshot>("get_vehicle_snapshot");
      setSnapshot(payload);
      setDriveMode(payload.driveMode);
      modeRef.current = payload.driveMode;
      return true;
    } catch (error) {
      console.warn("tauri invoke failed; using fallback telemetry", error);
      const fallback = buildFallbackSnapshot(modeRef.current, performance.now() / 1000);
      setSnapshot(fallback);
      return false;
    }
  }, [buildFallbackSnapshot]);

  useEffect(() => {
    if (runtime !== "tauri") return;
    let stopped = false;
    let timer: number | null = null;

    const tick = async () => {
      if (stopped) return;
      await fetchSnapshot();
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
