import type { VehicleSection } from "../../types/telemetry";

interface Props {
  sections: VehicleSection[];
  logs: string[];
  showLogs?: boolean;
}

export function VehicleStatePanel({ sections, logs, showLogs = true }: Props) {
  return (
    <div className="panel panel--vsr">
      <div className="panel__header">
        <h3>Vehicle State Report</h3>
        <p>Structured, high-rate telemetry</p>
      </div>
      <div className={`vsr-layout ${showLogs ? "" : "vsr-layout--no-logs"}`}>
        <div className="vsr-layout__sections">
          {sections.map(section => (
            <section key={section.id} className="vsr-section">
              <header>
                <h4>{section.title}</h4>
                <span>{section.fields.length} fields</span>
              </header>
              <div className="vsr-section__grid">
                {section.fields.map(field => (
                  <article key={`${section.id}-${field.label}`} className="vsr-field">
                    <span>{field.label}</span>
                    <strong>
                      {field.value}
                      {field.unit && <em>{field.unit}</em>}
                    </strong>
                  </article>
                ))}
              </div>
            </section>
          ))}
        </div>
        {showLogs && (
          <div className="panel__logs">
            <h4>Live Text Logs</h4>
            <ul>
              {logs.map((line, idx) => (
                <li key={`${line}-${idx}`}>{line}</li>
              ))}
            </ul>
          </div>
        )}
      </div>
    </div>
  );
}
