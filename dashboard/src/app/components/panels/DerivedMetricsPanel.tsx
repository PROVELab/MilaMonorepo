import type { DerivedMetric } from "../../types/telemetry";

interface Props {
  metrics: DerivedMetric[];
}

function Sparkline({ values }: { values: number[] }) {
  const width = 160;
  const height = 56;
  if (!values.length) {
    return <svg width={width} height={height} className="sparkline" />;
  }

  const max = Math.max(...values);
  const min = Math.min(...values);
  const range = max - min || 1;
  const points = values
    .map((value, index) => {
      const x = (index / (values.length - 1 || 1)) * width;
      const y = height - ((value - min) / range) * height;
      return `${x},${y}`;
    })
    .join(" ");

  return (
    <svg width={width} height={height} className="sparkline" aria-hidden>
      <polyline points={points} />
    </svg>
  );
}

export function DerivedMetricsPanel({ metrics }: Props) {
  return (
    <div className="panel panel--metrics">
      <div className="panel__header">
        <h3>Derived Metrics</h3>
        <p>Smoothed energy & efficiency windows</p>
      </div>
      <div className="metrics-grid">
        {metrics.map(metric => (
          <article key={metric.label} className="metric-card">
            <header>
              <span>{metric.label}</span>
              <strong>
                {metric.current} <em>{metric.unit}</em>
              </strong>
            </header>
            <Sparkline values={metric.sparkline} />
            <div className="metric-card__windows">
              {metric.windows.map(window => (
                <div key={window.label}>
                  <span>{window.label}</span>
                  <strong>{window.value}</strong>
                </div>
              ))}
            </div>
          </article>
        ))}
      </div>
    </div>
  );
}
