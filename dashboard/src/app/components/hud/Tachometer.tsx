import { MAX_DISPLAY_RPM } from "../../constants/vehicle";

interface TachometerProps {
  rpm: number | null;
  pedalPct: number | null;
}

function formatRpm(rpm: number | null): string {
  if (rpm == null) return "---";
  return Math.round(rpm).toLocaleString();
}

export function Tachometer({ rpm, pedalPct }: TachometerProps) {
  const normalizedRpm = rpm == null ? 0 : Math.min(Math.max(Math.abs(rpm), 0), MAX_DISPLAY_RPM) / MAX_DISPLAY_RPM;
  const normalizedPedal = pedalPct == null ? 0 : Math.min(100, Math.max(0, pedalPct));
  const radius = 90;
  const circumference = 2 * Math.PI * radius;
  const dashOffset = circumference * (1 - normalizedRpm);

  return (
    <div className="tachometer">
      <svg width="220" height="140" viewBox="0 0 220 140" aria-hidden="true">
        <defs>
          <linearGradient id="tach-gradient" x1="0%" x2="100%" y1="0%" y2="0%">
            <stop offset="0%" stopColor="#f0f5ef" />
            <stop offset="100%" stopColor="#72d7c5" />
          </linearGradient>
        </defs>
        <circle
          cx="110"
          cy="120"
          r={radius}
          fill="none"
          stroke="rgba(255,255,255,0.08)"
          strokeWidth={14}
          strokeDasharray={circumference}
          strokeDashoffset={circumference * 0.35}
          strokeLinecap="round"
          transform="rotate(-110 110 120)"
        />
        <circle
          className="tachometer__arc"
          cx="110"
          cy="120"
          r={radius}
          fill="none"
          stroke="url(#tach-gradient)"
          strokeWidth={14}
          strokeDasharray={circumference}
          strokeDashoffset={dashOffset}
          strokeLinecap="round"
          transform="rotate(-110 110 120)"
        />
      </svg>
      <div className="tachometer__readout">
        <span className="tachometer__label">Motor</span>
        <span className="tachometer__number">{formatRpm(rpm)}</span>
        <span className="tachometer__unit">RPM</span>
        <div className="pedal-indicator" aria-label={`Pedal position ${Math.round(normalizedPedal)} percent`}>
          <span>Pedal</span>
          <div className="pedal-indicator__bar">
            <div style={{ width: `${normalizedPedal}%` }} />
          </div>
        </div>
      </div>
    </div>
  );
}
