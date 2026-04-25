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

export interface VehicleSnapshot {
  speedMph: number;
  pedalPct: number; // 0 - 100
  torqueRatio: number; // 0 - 1
  batteryPct: number;
  driveMode: DriveMode;
  sections: VehicleSection[];
  liveTextLogs: string[];
}
