"use client";

interface PanelProps {
  title: string;
  children: React.ReactNode;
}

export function Panel({ title, children }: PanelProps) {
  return (
    <div className="panel-v2">
      <header className="panel-v2-header">
        <h2>{title}</h2>
      </header>
      <div className="panel-v2-content">{children}</div>
    </div>
  );
}
