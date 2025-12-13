export type DriveMode = "P" | "D" | "R";

export interface ImuReading {
  pitch: number; // degrees
  roll: number; // degrees
  yaw: number; // degrees
}

export interface VehicleField {
  label: string;
  value: string;
  unit?: string;
}

export interface VehicleSection {
  id: string;
  title: string;
  fields: VehicleField[];
}

export interface MetricWindow {
  label: string; // e.g. 5m, 10m, 30m
  value: string;
}

export interface DerivedMetric {
  label: string;
  unit: string;
  current: string;
  windows: MetricWindow[];
  sparkline: number[];
}

export interface VehicleSnapshot {
  speedMph: number;
  torqueRatio: number; // 0 - 1
  batteryPct: number;
  driveMode: DriveMode;
  imu: ImuReading;
  sections: VehicleSection[];
  liveTextLogs: string[];
  derivedMetrics: DerivedMetric[];
}
