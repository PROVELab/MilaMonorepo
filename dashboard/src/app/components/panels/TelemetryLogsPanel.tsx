interface Props {
  logs: string[];
}

export function TelemetryLogsPanel({ logs }: Props) {
  return (
    <div className="panel panel--logs">
      <div className="panel__header">
        <h3>Live Event Stream</h3>
        <p>Priority signals and system notices</p>
      </div>
      <div className="panel__logs panel__logs--full">
        <h4>Telemetry Logs</h4>
        <ul>
          {logs.map((line, idx) => (
            <li key={`${line}-${idx}`}>{line}</li>
          ))}
        </ul>
      </div>
    </div>
  );
}
