export type DriveMode = "P" | "D" | "R";

export interface VehicleField {
  key: string;
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

export interface FieldTrendPoint {
  minutesFromNow: number;
  value: number;
  source: string;
}

export interface FieldPrediction {
  horizonMinutes: number;
  value: number;
}

export interface FieldTrendResponse {
  sectionId: string;
  fieldKey: string;
  label: string;
  unit?: string;
  fitDegree: number;
  rawPoints: FieldTrendPoint[];
  fitPoints: FieldTrendPoint[];
  predictions: FieldPrediction[];
}
