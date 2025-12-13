interface TachometerProps {
  speed: number;
  torqueRatio: number;
}

const MAX_SPEED = 160;

export function Tachometer({ speed, torqueRatio }: TachometerProps) {
  const normalizedSpeed = Math.min(Math.max(Math.abs(speed), 0), MAX_SPEED) / MAX_SPEED;
  const radius = 90;
  const circumference = 2 * Math.PI * radius;
  const dashOffset = circumference * (1 - normalizedSpeed);

  return (
    <div className="tachometer">
      <svg width="220" height="140" viewBox="0 0 220 140">
        <defs>
          <linearGradient id="tach-gradient" x1="0%" x2="100%" y1="0%" y2="0%">
            <stop offset="0%" stopColor="#00ffbd" />
            <stop offset="100%" stopColor="#30a5ff" />
          </linearGradient>
        </defs>
        <circle
          className="tachometer__track"
          cx="110"
          cy="120"
          r={radius}
          fill="none"
          stroke="rgba(255,255,255,0.08)"
          strokeWidth={14}
          strokeDasharray={circumference}
          strokeDashoffset={circumference * 0.35}
          pathLength={1}
        />
        <circle
          className="tachometer__value"
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
        <span className="tachometer__label">Speed</span>
        <span className="tachometer__value">{Math.round(speed)}</span>
        <span className="tachometer__unit">mph</span>
      </div>
      <div className="torque-indicator">
        <span>Torque</span>
        <div className="torque-indicator__bar">
          <div style={{ width: `${Math.min(1, Math.max(0, torqueRatio)) * 100}%` }} />
        </div>
      </div>
    </div>
  );
}
