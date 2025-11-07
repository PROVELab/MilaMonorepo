"use client";

interface Props {
  onEngage?: () => void;
}

export function EmergencyStopButton({ onEngage }: Props) {
  return (
    <button
      type="button"
      aria-label="Emergency software shutoff"
      className="emergency-stop"
      onClick={() => {
        if (onEngage) onEngage();
        console.warn("Emergency shutdown requested");
      }}
    >
      <span className="emergency-stop__pulse" />
      <span>Emergency Shutoff</span>
    </button>
  );
}
