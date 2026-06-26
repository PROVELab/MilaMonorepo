"use client";

import { invoke } from "@tauri-apps/api/core";
import { useEffect, useMemo, useState } from "react";
import dynamic from "next/dynamic";
import type { FieldTrendResponse, VehicleField, VehicleSection } from "../../types/telemetry";

const TrendChart = dynamic(() => import("./TrendChart"), {
  ssr: false,
  loading: () => <p className="trend-modal__status">Loading chart...</p>
});

interface Props {
  sections: VehicleSection[];
  logs: string[];
}

interface ActiveField {
  sectionId: string;
  sectionTitle: string;
  field: VehicleField;
}

export function VehicleStatePanel({ sections, logs }: Props) {
  const [activeField, setActiveField] = useState<ActiveField | null>(null);
  const [trend, setTrend] = useState<FieldTrendResponse | null>(null);
  const [trendLoading, setTrendLoading] = useState(false);
  const [trendRefreshing, setTrendRefreshing] = useState(false);
  const [trendError, setTrendError] = useState<string | null>(null);
  const [trendUpdatedAtMs, setTrendUpdatedAtMs] = useState<number | null>(null);
  const [refreshTrigger, setRefreshTrigger] = useState(0);

  const closeTrend = () => {
    setActiveField(null);
    setTrend(null);
    setTrendError(null);
    setTrendLoading(false);
    setTrendRefreshing(false);
    setTrendUpdatedAtMs(null);
  };

  const requestTrend = (section: VehicleSection, field: VehicleField) => {
    setActiveField({ sectionId: section.id, sectionTitle: section.title, field });
    setTrend(null);
    setTrendError(section.id === "mcu-link" ? "Trend analysis is available for VSR payload fields." : null);
    setTrendLoading(false);
    setTrendRefreshing(false);
    setTrendUpdatedAtMs(null);
    setRefreshTrigger(0);
  };

  useEffect(() => {
    if (!activeField || activeField.sectionId === "mcu-link") return;

    let cancelled = false;
    let inFlight = false;
    let firstFetch = refreshTrigger === 0;
    const sectionId = activeField.sectionId;
    const fieldKey = activeField.field.key;

    const fetchTrend = async () => {
      if (cancelled || inFlight) return;
      inFlight = true;
      if (firstFetch) {
        setTrendLoading(true);
      } else {
        setTrendRefreshing(true);
      }

      try {
        const data = await invoke<FieldTrendResponse>("get_vsr_field_analysis", { sectionId, fieldKey });
        if (!cancelled) {
          setTrend(data);
          setTrendError(null);
          setTrendUpdatedAtMs(Date.now());
        }
      } catch (error) {
        if (!cancelled) setTrendError(String(error));
      } finally {
        if (!cancelled) {
          setTrendLoading(false);
          setTrendRefreshing(false);
          firstFetch = false;
        }
        inFlight = false;
      }
    };

    void fetchTrend();
    return () => {
      cancelled = true;
    };
  }, [activeField, refreshTrigger]);

  const updatedLabel = useMemo(() => {
    if (!trendUpdatedAtMs) return "not yet";
    return new Date(trendUpdatedAtMs).toLocaleTimeString();
  }, [trendUpdatedAtMs]);

  return (
    <>
      <div className="panel panel--vsr">
        <header className="panel__header">
          <h3>Vehicle State</h3>
          <p>Live VSR fields and backend logs</p>
        </header>
        <div className="vsr-layout">
          <div className="vsr-layout__sections">
            {sections.map(section => (
              <section key={section.id} className="vsr-section">
                <header>
                  <h4>{section.title}</h4>
                  <span>{section.fields.length} fields</span>
                </header>
                <div className="vsr-section__grid">
                  {section.fields.map(field => (
                    <button
                      key={`${section.id}-${field.key}`}
                      type="button"
                      className="vsr-field vsr-field--button"
                      onClick={() => requestTrend(section, field)}
                    >
                      <span>{field.label}</span>
                      <strong>
                        {field.value}
                        {field.unit && <em>{field.unit}</em>}
                      </strong>
                    </button>
                  ))}
                </div>
              </section>
            ))}
          </div>
          <aside className="panel__logs">
            <h4>Logs</h4>
            <ul>
              {logs.map((line, idx) => (
                <li key={`${line}-${idx}`}>{line}</li>
              ))}
            </ul>
          </aside>
        </div>
      </div>

      {activeField && (
        <div className="trend-modal__backdrop" onClick={closeTrend}>
          <div className="trend-modal" onClick={event => event.stopPropagation()}>
            <header className="trend-modal__header">
              <div>
                <h4>{activeField.field.label}</h4>
                <p>
                  {activeField.sectionTitle}.{activeField.field.key}
                </p>
              </div>
              <button type="button" className="trend-modal__close" onClick={closeTrend}>
                Close
              </button>
            </header>
            <div className="trend-modal__submeta">
              <span>Linear trend</span>
              <span>Last update: {updatedLabel}</span>
              {trendRefreshing ? <span>Refreshing</span> : null}
              <button
                type="button"
                onClick={() => setRefreshTrigger(r => r + 1)}
                style={{
                  background: "transparent",
                  border: "1px solid var(--line-strong)",
                  color: "var(--foreground)",
                  borderRadius: "4px",
                  padding: "2px 8px",
                  cursor: "pointer",
                  fontSize: "12px"
                }}
              >
                Refresh
              </button>
            </div>
            {trendLoading && <p className="trend-modal__status">Loading trend data...</p>}
            {trendError && <p className="trend-modal__status trend-modal__status--error">{trendError}</p>}
            {trend ? <TrendChart trend={trend} /> : null}
          </div>
        </div>
      )}
    </>
  );
}
