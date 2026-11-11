import type { DriveMode } from "../types/telemetry";

export const DRIVE_MODE_GAMEPAD_INPUTS = [
  { button: 12, label: "D-pad Up", mode: "Drive" },
  { button: 13, label: "D-pad Down", mode: "Reverse" },
  { button: 14, label: "D-pad Left", mode: "Park" },
  { button: 15, label: "D-pad Right", mode: "Neutral" },
] as const satisfies ReadonlyArray<{
  button: number;
  label: string;
  mode: DriveMode;
}>;
