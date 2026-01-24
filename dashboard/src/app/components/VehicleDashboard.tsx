"use client";

import { useMemo, useState } from "react";
import type { DriveMode, ImuReading } from "../types/telemetry";
import { VehicleScene } from "./VehicleScene";
import { Tachometer } from "./hud/Tachometer";
import { NavBall } from "./hud/NavBall";
import { DriveModeSelector } from "./hud/DriveModeSelector";
import { EmergencyStopButton } from "./hud/EmergencyStopButton";
import { VehicleStatePanel } from "./panels/VehicleStatePanel";
import { ReverseCameraPanel } from "./panels/ReverseCameraPanel";
import { DerivedMetricsPanel } from "./panels/DerivedMetricsPanel";
import { TelemetryLogsPanel } from "./panels/TelemetryLogsPanel";
import { useVehicleTelemetry } from "../hooks/useVehicleTelemetry";

type DockTab = "raw" | "metrics" | "logs";

interface DriveViewProps {
  batteryPct: number;
  driveMode: DriveMode;
  speedMph: number;
  torqueRatio: number;
  imu: ImuReading;
}

function DriveView({ batteryPct, driveMode, speedMph, torqueRatio, imu }: DriveViewProps) {
  return (
    <div className="drive-view">
      <VehicleScene speed={speedMph} driveMode={driveMode} />
      <div className="drive-view__overlay">
        <div className="drive-view__hud-left">
          <Tachometer speed={speedMph} torqueRatio={torqueRatio} />
          <div className="hud-badge">
            <span>Battery</span>
            <strong>{batteryPct.toFixed(1)}%</strong>
          </div>
        </div>
        <div className="drive-view__hud-right">
          <NavBall imu={imu} />
          <div className="hud-badge">
            <span>Drive Mode</span>
            <strong>{driveMode}</strong>
          </div>
        </div>
      </div>
    </div>
  );
}

export function VehicleDashboard() {
  const { snapshot, driveMode, changeDriveMode } = useVehicleTelemetry(180);
  const [activeDock, setActiveDock] = useState<DockTab>("raw");
  const [dockOpen, setDockOpen] = useState(true);
  const [cameraActive, setCameraActive] = useState(false);

  const summaryCards = useMemo(
    () => [
      { label: "Speed", value: `${snapshot.speedMph.toFixed(1)} mph` },
      { label: "Battery", value: `${snapshot.batteryPct.toFixed(1)}%` },
      { label: "Torque", value: `${Math.round(snapshot.torqueRatio * 100)}%` },
    ],
    [snapshot],
  );

  return (
    <div className={`dashboard-shell ${dockOpen ? "dock-open" : ""}`}>
      <header className="topbar">
        <div className="topbar__brand">
          <span>Prove EV</span>
          <small>Vehicle Operations Console</small>
        </div>
        <div className="topbar__status">
          <span className="status-chip">Mode {driveMode}</span>
          <span className="status-chip">{snapshot.speedMph.toFixed(1)} mph</span>
          <span className="status-chip">SOC {snapshot.batteryPct.toFixed(1)}%</span>
        </div>
        <div className="topbar__actions">
          <button type="button" className="action-button" onClick={() => setCameraActive(true)}>
            Reverse Camera
          </button>
        </div>
      </header>

      <section className="dashboard-main">
        <aside className="control-rail">
          <div className="rail-card rail-card--stack">
            <h4>Drive Select</h4>
            <div className="drive-select-card">
              <div className="drive-select-card__status">
                <span>Mode</span>
                <strong>{driveMode}</strong>
              </div>
              <DriveModeSelector value={driveMode} onChange={changeDriveMode} />
            </div>
          </div>
          <div className="rail-card">
            <h4>Critical Actions</h4>
            <EmergencyStopButton />
          </div>
          <div className="rail-card">
            <h4>Quick Stats</h4>
            <div className="rail-stats">
              {summaryCards.map(card => (
                <div key={card.label}>
                  <span>{card.label}</span>
                  <strong>{card.value}</strong>
                </div>
              ))}
            </div>
          </div>
          <div className="rail-card rail-card--metrics">
            <h4>Efficiency Window</h4>
            <div className="rail-metric">
              <span>{snapshot.derivedMetrics[0]?.label ?? "Energy"}</span>
              <strong>
                {snapshot.derivedMetrics[0]?.current ?? "--"}{" "}
                <em>{snapshot.derivedMetrics[0]?.unit ?? ""}</em>
              </strong>
            </div>
            <div className="rail-metric">
              <span>{snapshot.derivedMetrics[1]?.label ?? "Regen"}</span>
              <strong>
                {snapshot.derivedMetrics[1]?.current ?? "--"}{" "}
                <em>{snapshot.derivedMetrics[1]?.unit ?? ""}</em>
              </strong>
            </div>
          </div>
          <div className="rail-card rail-card--statusline">
            <h4>System Pulse</h4>
            <div className="status-line">
              <span>{snapshot.liveTextLogs[0] ?? "telemetry buffer warming up"}</span>
            </div>
          </div>
        </aside>

        <div className="drive-stage">
          <DriveView
            batteryPct={snapshot.batteryPct}
            driveMode={driveMode}
            speedMph={snapshot.speedMph}
            torqueRatio={snapshot.torqueRatio}
            imu={snapshot.imu}
          />
        </div>

      </section>

      <section className={`data-dock ${dockOpen ? "" : "data-dock--collapsed"}`}>
        <div className="dock-header">
          <div className="dock-title">
            <span>Telemetry Dock</span>
            <small>Raw signals and derived trends</small>
          </div>
          <div className="dock-tabs">
            <button type="button" className={activeDock === "raw" ? "is-active" : ""} onClick={() => setActiveDock("raw")}>
              Raw Data
            </button>
            <button
              type="button"
              className={activeDock === "metrics" ? "is-active" : ""}
              onClick={() => setActiveDock("metrics")}
            >
              Metrics
            </button>
            <button type="button" className={activeDock === "logs" ? "is-active" : ""} onClick={() => setActiveDock("logs")}>
              Logs
            </button>
            <button type="button" className="dock-toggle" onClick={() => setDockOpen(false)}>
              Close
            </button>
          </div>
        </div>
        {dockOpen && (
          <div className="dock-body">
            {activeDock === "raw" && (
              <VehicleStatePanel sections={snapshot.sections} logs={snapshot.liveTextLogs} showLogs={false} />
            )}
            {activeDock === "metrics" && <DerivedMetricsPanel metrics={snapshot.derivedMetrics} />}
            {activeDock === "logs" && <TelemetryLogsPanel logs={snapshot.liveTextLogs} />}
          </div>
        )}
      </section>

      {!dockOpen && (
        <button type="button" className="dock-handle" onClick={() => setDockOpen(true)}>
          <span>Telemetry Dock</span>
          <em>Tap to expand</em>
        </button>
      )}

      {cameraActive && (
        <div className="camera-overlay">
          <div className="camera-overlay__header">
            <div>
              <strong>Reverse Camera</strong>
              <span>Full-screen feed</span>
            </div>
            <button type="button" className="action-button" onClick={() => setCameraActive(false)}>
              Close
            </button>
          </div>
          <div className="camera-overlay__body">
            <ReverseCameraPanel />
          </div>
        </div>
      )}
    </div>
  );
}
