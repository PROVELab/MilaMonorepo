"use client";

interface Props {
  active: boolean;
  targetRpm: number | null;
  onMinus?: () => void;
  onPlus?: () => void;
}

export function CruiseControlPanel({ active, targetRpm, onMinus, onPlus }: Props) {
  const canAdjust = active && targetRpm != null;

  return (
    <div className={`cruise-control ${active ? "cruise-control--active" : ""}`}>
      <span className="cruise-control__label">Cruise</span>
      <strong className="cruise-control__target">
        {targetRpm == null ? "---" : Math.round(targetRpm).toLocaleString()}
        <em>RPM</em>
      </strong>
      <div className="cruise-control__buttons">
        <button type="button" onClick={onMinus} disabled={!canAdjust} aria-label="Decrease cruise target RPM">
          -
        </button>
        <button type="button" onClick={onPlus} disabled={!canAdjust} aria-label="Increase cruise target RPM">
          +
        </button>
      </div>
    </div>
  );
}
