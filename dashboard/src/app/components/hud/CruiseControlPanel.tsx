"use client";

interface Props {
  active: boolean;
  targetRpm: number | null;
  onDecrease10?: () => void;
  onDecrease50?: () => void;
  onIncrease10?: () => void;
  onIncrease50?: () => void;
}

export function CruiseControlPanel({
  active,
  targetRpm,
  onDecrease10,
  onDecrease50,
  onIncrease10,
  onIncrease50,
}: Props) {
  const canAdjust = active && targetRpm != null;

  return (
    <div className={`cruise-control ${active ? "cruise-control--active" : ""}`}>
      <span className="cruise-control__label">Cruise</span>
      <strong className="cruise-control__target">
        {targetRpm == null ? "---" : Math.round(targetRpm).toLocaleString()}
        <em>RPM</em>
      </strong>
      <div className="cruise-control__buttons">
        <button
          type="button"
          onClick={onDecrease50}
          disabled={!canAdjust}
          aria-label="Decrease cruise target RPM by 50"
        >
          -50
        </button>
        <button
          type="button"
          onClick={onDecrease10}
          disabled={!canAdjust}
          aria-label="Decrease cruise target RPM by 10"
        >
          -10
        </button>
        <button
          type="button"
          onClick={onIncrease10}
          disabled={!canAdjust}
          aria-label="Increase cruise target RPM by 10"
        >
          +10
        </button>
        <button
          type="button"
          onClick={onIncrease50}
          disabled={!canAdjust}
          aria-label="Increase cruise target RPM by 50"
        >
          +50
        </button>
      </div>
    </div>
  );
}
