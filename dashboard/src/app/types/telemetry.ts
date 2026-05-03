export type DriveMode = "Reverse" | "Park" | "Neutral" | "Drive" | "Cruise Control";

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
  motorRpm: number | null;
  pedalPct: number | null;
  brakePct: number | null;
  driveMode: DriveMode;
  cruiseTargetRpm?: number | null;
  sections: VehicleSection[];
  liveTextLogs: string[];
  isSerialReady: boolean;
  framesReceived: number;
  lastFrameAgeSeconds?: number | null;
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
