"use client";

interface Props {
  active: boolean;
  targetMph: number | null;
  onMinus?: () => void;
  onPlus?: () => void;
}

export function CruiseControlPanel({ active, targetMph, onMinus, onPlus }: Props) {
  const canAdjust = active && targetMph != null;

  return (
    <div className={`cruise-control ${active ? "cruise-control--active" : ""}`}>
      <span className="cruise-control__label">Cruise</span>
      <strong className="cruise-control__target">
        {targetMph == null ? "--.-" : targetMph.toFixed(1)}
        <em>mph</em>
      </strong>
      <div className="cruise-control__buttons">
        <button type="button" onClick={onMinus} disabled={!canAdjust} aria-label="Decrease cruise target">
          -
        </button>
        <button type="button" onClick={onPlus} disabled={!canAdjust} aria-label="Increase cruise target">
          +
        </button>
      </div>
    </div>
  );
}
