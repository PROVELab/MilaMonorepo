import type { ImuReading } from "../../types/telemetry";

interface Props {
  imu: ImuReading;
}

export function NavBall({ imu }: Props) {
  const radius = 110;
  const center = 110;
  const pitchScale = radius / 90;
  const pitchMarks = [10, 20, 30, 40, 50, 60, 70, 80];
  const rollMarks = [10, 20, 30, 40, 50, 60, 70, 80, 90];

  return (
    <div className="navball">
      <div className="navball__sphere">
        <div
          className="navball__horizon"
          style={{
            transform: `translateY(${imu.pitch * -1.4}px) rotate(${imu.roll}deg)`,
          }}
        />
        <div className="navball__yaw" style={{ transform: `rotate(${imu.yaw}deg)` }} />

        <svg className="navball__overlay" viewBox="0 0 220 220">
          {pitchMarks.flatMap(mark => [mark, -mark]).map(mark => {
            const y = center - mark * pitchScale;
            const label = Math.abs(mark / 10);
            return (
              <g key={`pitch-${mark}`}>
                <line x1={center - 70} x2={center + 70} y1={y} y2={y} />
                <text x={center + 74} y={y + 4}>
                  {label}
                </text>
                <text x={center - 86} y={y + 4}>
                  {label}
                </text>
              </g>
            );
          })}

          {rollMarks.flatMap(mark => [mark, -mark]).map(mark => {
            const radians = (mark * Math.PI) / 180;
            const inner = radius - 18;
            const outer = radius - 6;
            const sx = center + inner * Math.sin(radians);
            const sy = center - inner * Math.cos(radians);
            const ex = center + outer * Math.sin(radians);
            const ey = center - outer * Math.cos(radians);
            const labelRadius = radius - 32;
            const lx = center + labelRadius * Math.sin(radians);
            const ly = center - labelRadius * Math.cos(radians);
            const label = Math.abs(mark / 10);
            const showLabel = Math.abs(mark) % 30 === 0;
            return (
              <g key={`roll-${mark}`}>
                <line x1={sx} y1={sy} x2={ex} y2={ey} />
                {showLabel && (
                  <text x={lx} y={ly} transform={`rotate(${mark}, ${lx}, ${ly})`}>
                    {label}
                  </text>
                )}
              </g>
            );
          })}
        </svg>
      </div>
      <div className="navball__metrics">
        <div>
          <span>Pitch</span>
          <strong>{imu.pitch.toFixed(1)}°</strong>
        </div>
        <div>
          <span>Roll</span>
          <strong>{imu.roll.toFixed(1)}°</strong>
        </div>
        <div>
          <span>Yaw</span>
          <strong>{imu.yaw.toFixed(1)}°</strong>
        </div>
      </div>
    </div>
  );
}
