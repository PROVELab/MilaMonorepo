export type DriveMode = "P" | "D" | "R";

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
  label: string;
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
  sections: VehicleSection[];
  liveTextLogs: string[];
}
