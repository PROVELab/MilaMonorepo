"use client";

import { invoke } from "@tauri-apps/api/core";
import { useEffect, useMemo, useState } from "react";
import type {
  FieldTrendPoint,
  FieldTrendResponse,
  VehicleField,
  VehicleSection,
} from "../../types/telemetry";

interface Props {
  sections: VehicleSection[];
  logs: string[];
}

interface ActiveField {
  sectionId: string;
  sectionTitle: string;
  field: VehicleField;
}

interface Scale {
  domainMin: number;
  domainMax: number;
  map: (value: number) => number;
}

function createLinearScale(domainMin: number, domainMax: number, rangeMin: number, rangeMax: number): Scale {
  const span = Math.max(1e-9, domainMax - domainMin);
  return {
    domainMin,
    domainMax,
    map: (value: number) => rangeMin + ((value - domainMin) / span) * (rangeMax - rangeMin),
  };
}

function buildPath(points: FieldTrendPoint[], xScale: Scale, yScale: Scale): string {
  if (!points.length) return "";
  return points
    .map((point, index) => `${index === 0 ? "M" : "L"} ${xScale.map(point.minutesFromNow).toFixed(2)} ${yScale.map(point.value).toFixed(2)}`)
    .join(" ");
}

function evenlySpacedTicks(min: number, max: number, count: number): number[] {
  if (count <= 1) return [min];
  const step = (max - min) / (count - 1);
  return Array.from({ length: count }, (_, index) => min + index * step);
}

function formatAxisTick(value: number, decimals: number): string {
  return value.toFixed(decimals);
}

function formatXTick(minutes: number): string {
  if (Math.abs(minutes) < 0.001) return "Now";
  if (minutes > 0) return `+${Math.round(minutes)}m`;
  return `${Math.round(minutes)}m`;
}

function TrendChart({ trend }: { trend: FieldTrendResponse }) {
  const width = 860;
  const height = 360;
  const left = 78;
  const right = 24;
  const top = 22;
  const bottom = 52;

  const sortedRaw = useMemo(
    () => [...trend.rawPoints].sort((a, b) => a.minutesFromNow - b.minutesFromNow),
    [trend.rawPoints],
  );
  const sortedFit = useMemo(
    () => [...trend.fitPoints].sort((a, b) => a.minutesFromNow - b.minutesFromNow),
    [trend.fitPoints],
  );

  if (!sortedRaw.length && !sortedFit.length) {
    return <div className="trend-modal__empty">No points available</div>;
  }

  const historicalXMin = sortedRaw.length
    ? Math.min(...sortedRaw.map(point => point.minutesFromNow))
    : -1;
  const xMin = Math.min(-1, historicalXMin);
  const xMax = 30;
  const xScale = createLinearScale(xMin, xMax, left, width - right);

  const historicalValues = sortedRaw.map(point => point.value);
  const fallbackValues = sortedFit.filter(point => point.minutesFromNow <= 0).map(point => point.value);
  const yBase = historicalValues.length > 0 ? historicalValues : fallbackValues.length > 0 ? fallbackValues : [0];
  const yRawMin = Math.min(...yBase);
  const yRawMax = Math.max(...yBase);
  const ySpan = Math.max(1e-9, yRawMax - yRawMin);
  const yPad = ySpan * 0.12 + Math.max(1e-6, Math.abs(yRawMax) * 0.02);
  const yMin = yRawMin - yPad;
  const yMax = yRawMax + yPad;
  const yScale = createLinearScale(yMin, yMax, height - bottom, top);

  const fitHistory = sortedFit.filter(point => point.minutesFromNow <= 0);
  const fitForecast = sortedFit.filter(point => point.minutesFromNow >= 0);
  const yTickDecimals = ySpan >= 100 ? 0 : ySpan >= 10 ? 1 : ySpan >= 1 ? 2 : 3;
  const yTicks = evenlySpacedTicks(yMin, yMax, 5);

  const xTickCandidates = [-30, -15, -10, -5, -2, -1, 0, 5, 10, 15, 30];
  const xTicks = xTickCandidates.filter(value => value >= xMin && value <= xMax);
  const nowX = xScale.map(0);
  const forecastOutOfRange = trend.predictions.some(pred => pred.value < yMin || pred.value > yMax);

  return (
    <div className="trend-modal__chart-wrap">
      <svg viewBox={`0 0 ${width} ${height}`} className="trend-modal__chart" role="img" aria-label="Historical trend and forecast">
        <rect x={0} y={0} width={width} height={height} rx={18} ry={18} fill="rgba(5, 7, 12, 0.92)" />
        <rect
          x={nowX}
          y={top}
          width={Math.max(0, width - right - nowX)}
          height={height - top - bottom}
          className="trend-modal__future-zone"
        />

        {yTicks.map(tick => (
          <line
            key={`y-grid-${tick}`}
            x1={left}
            x2={width - right}
            y1={yScale.map(tick)}
            y2={yScale.map(tick)}
            className="trend-modal__grid-line"
          />
        ))}

        {xTicks.map(tick => (
          <line
            key={`x-grid-${tick}`}
            x1={xScale.map(tick)}
            x2={xScale.map(tick)}
            y1={top}
            y2={height - bottom}
            className={Math.abs(tick) < 0.001 ? "trend-modal__grid-line trend-modal__grid-line--now" : "trend-modal__grid-line"}
          />
        ))}

        <line x1={left} y1={top} x2={left} y2={height - bottom} className="trend-modal__axis" />
        <line x1={left} y1={height - bottom} x2={width - right} y2={height - bottom} className="trend-modal__axis" />
        <line x1={nowX} x2={nowX} y1={top} y2={height - bottom} className="trend-modal__now-line" />

        <path d={buildPath(fitHistory, xScale, yScale)} className="trend-modal__fit-line" />
        <path d={buildPath(fitForecast, xScale, yScale)} className="trend-modal__fit-line trend-modal__fit-line--forecast" />

        {sortedRaw.map((point, idx) => (
          <circle
            key={`${point.minutesFromNow}-${point.value}-${idx}`}
            cx={xScale.map(point.minutesFromNow)}
            cy={yScale.map(point.value)}
            r={point.source === "live" ? 3.4 : 2.4}
            className={point.source === "live" ? "trend-modal__point trend-modal__point--live" : "trend-modal__point"}
          />
        ))}

        {yTicks.map(tick => (
          <text
            key={`y-label-${tick}`}
            x={left - 8}
            y={yScale.map(tick) + 4}
            textAnchor="end"
            className="trend-modal__tick-label"
          >
            {formatAxisTick(tick, yTickDecimals)}
          </text>
        ))}

        {xTicks.map(tick => (
          <text
            key={`x-label-${tick}`}
            x={xScale.map(tick)}
            y={height - bottom + 18}
            textAnchor="middle"
            className="trend-modal__tick-label"
          >
            {formatXTick(tick)}
          </text>
        ))}

        <text x={left} y={height - 12} className="trend-modal__axis-label">
          Time (minutes from now)
        </text>
        <text x={left} y={12} className="trend-modal__axis-label">
          Value{trend.unit ? ` (${trend.unit})` : ""}
        </text>
      </svg>

      <div className="trend-modal__legend">
        <span><i className="trend-modal__legend-dot" />Raw points</span>
        <span><i className="trend-modal__legend-dot trend-modal__legend-dot--live" />Live point</span>
        <span><i className="trend-modal__legend-line" />Curve fit (history)</span>
        <span><i className="trend-modal__legend-line trend-modal__legend-line--forecast" />Forecast extrapolation</span>
      </div>

      {forecastOutOfRange ? (
        <p className="trend-modal__note">
          Forecast values extend beyond the historical y-axis scale.
        </p>
      ) : null}
    </div>
  );
}

export function VehicleStatePanel({ sections, logs }: Props) {
  const [activeField, setActiveField] = useState<ActiveField | null>(null);
  const [trend, setTrend] = useState<FieldTrendResponse | null>(null);
  const [trendLoading, setTrendLoading] = useState(false);
  const [trendRefreshing, setTrendRefreshing] = useState(false);
  const [trendError, setTrendError] = useState<string | null>(null);
  const [trendUpdatedAtMs, setTrendUpdatedAtMs] = useState<number | null>(null);

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
    setTrendError(null);
    setTrendLoading(false);
    setTrendRefreshing(false);
    setTrendUpdatedAtMs(null);

    if (section.id === "mcu-link") {
      setTrendError("Trend analysis is only available for MCU VSR payload fields.");
    }
  };

  useEffect(() => {
    if (!activeField || activeField.sectionId === "mcu-link") {
      return;
    }

    let cancelled = false;
    let inFlight = false;
    let firstFetch = true;
    const sectionId = activeField.sectionId;
    const fieldKey = activeField.field.key;

    const fetchTrend = async () => {
      if (cancelled || inFlight) {
        return;
      }
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
        if (!cancelled) {
          setTrendError(String(error));
        }
      } finally {
        if (!cancelled) {
          if (firstFetch) {
            setTrendLoading(false);
            firstFetch = false;
          }
          setTrendRefreshing(false);
        }
        inFlight = false;
      }
    };

    void fetchTrend();
    const intervalId = window.setInterval(() => {
      void fetchTrend();
    }, 1000);

    return () => {
      cancelled = true;
      window.clearInterval(intervalId);
    };
  }, [activeField?.sectionId, activeField?.field.key]);

  const updatedLabel = useMemo(() => {
    if (!trendUpdatedAtMs) return "not yet";
    return new Date(trendUpdatedAtMs).toLocaleTimeString();
  }, [trendUpdatedAtMs]);

  return (
    <>
      <div className="panel panel--vsr">
        <div className="panel__header">
          <h3>Vehicle State Report</h3>
          <p>Structured, high-rate telemetry</p>
        </div>
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
          <div className="panel__logs">
            <h4>Live Text Logs</h4>
            <ul>
              {logs.map((line, idx) => (
                <li key={`${line}-${idx}`}>{line}</li>
              ))}
            </ul>
          </div>
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
              <span>Live polling: 1s</span>
              <span>Last update: {updatedLabel}</span>
              {trendRefreshing ? <span>Refreshing...</span> : null}
            </div>

            {trendLoading && <p className="trend-modal__status">Loading trend data...</p>}
            {trendError && <p className="trend-modal__status trend-modal__status--error">{trendError}</p>}

            {trend && (
              <>
                <TrendChart trend={trend} />
                <div className="trend-modal__meta">Polynomial fit degree: {trend.fitDegree}</div>
                <div className="trend-modal__predictions">
                  {trend.predictions.map(prediction => (
                    <article key={prediction.horizonMinutes} className="trend-modal__prediction-card">
                      <span>{prediction.horizonMinutes}m forecast</span>
                      <strong>
                        {prediction.value.toFixed(2)}
                        {trend.unit ? <em>{trend.unit}</em> : null}
                      </strong>
                    </article>
                  ))}
                </div>
              </>
            )}
          </div>
        </div>
      )}
    </>
  );
}
