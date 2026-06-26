# Dashboard Frontend

This is the tablet-facing dashboard UI.

The frontend has three operator views:

- Drive: 3D vehicle scene, RPM, pedal, mode controls, cruise RPM, emergency stop.
- VSR: live decoded VSR fields, log history, and per-field trend charts.
- Camera: reserved reverse-camera surface.

Telemetry rules:

- Show data only when it is sourced from the backend VSR snapshot.
- Do not invent vehicle values in the browser.
- Motor speed is displayed as RPM until MPH is added to the VSR.
- Drive mode reflects the VSR stream, not optimistic button state.
- Logs come from the backend live text log buffer.

Command rules:

- Mode buttons call the Tauri `send_motor_command` command.
- Cruise control sends `Cruise Control` plus a target RPM.
- Emergency stop calls `engage_emergency_stop`.
- The dashboard does not enforce drive-state transition rules.

File layout:

- `VehicleDashboard.tsx` composes the three main views.
- `hooks/useVehicleTelemetry.ts` polls backend snapshots and sends commands.
- `hooks/useLogToasts.ts` turns new backend log lines into temporary toasts.
- `components/dashboard` contains dashboard-level shell/view pieces.
- `components/hud` contains touch controls and live drive readouts.
- `components/panels` contains VSR/log/camera panels.
- `types/telemetry.ts` mirrors Tauri JSON response shapes.

VSR charts use Recharts.

The backend returns raw history points and a linear trendline. The chart opens on historical data ending near now. Future trend points are still present and can be reached with the chart brush.

The UI is sized for a large 15.6 inch tablet. Controls should remain large, stable, and touch-friendly.
