"use client";

export type DashboardViewId = "drive" | "vsr" | "reverse";

interface Props {
  activeView: DashboardViewId;
  onChange: (view: DashboardViewId) => void;
}

const VIEWS: Array<{ id: DashboardViewId; label: string }> = [
  { id: "drive", label: "Drive" },
  { id: "vsr", label: "VSR" },
  { id: "reverse", label: "Camera" },
];

export function ViewNav({ activeView, onChange }: Props) {
  return (
    <nav className="view-nav" aria-label="Dashboard views">
      {VIEWS.map(view => (
        <button
          key={view.id}
          type="button"
          className={view.id === activeView ? "is-active" : ""}
          onClick={() => onChange(view.id)}
        >
          {view.label}
        </button>
      ))}
    </nav>
  );
}
