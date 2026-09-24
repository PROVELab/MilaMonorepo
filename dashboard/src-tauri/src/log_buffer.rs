use std::collections::VecDeque;
use std::fs::OpenOptions;
use std::io::Write;
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

pub const MAX_LOG_LINES: usize = 120;

pub fn push_log_line(logs: &mut VecDeque<String>, message: impl AsRef<str>) {
    if logs.len() >= MAX_LOG_LINES {
        logs.pop_back();
    }

    let timestamp = wall_clock_timestamp();
    let line = format!("[{timestamp}] {}", message.as_ref());
    logs.push_front(line.clone());
    append_to_log_file(&line);
}

// Plain-text companion to the MCAP VSR recording.  Keep it out of the Tauri
// source tree: the development file watcher restarts the dashboard when a file
// under `src-tauri` changes.
fn append_to_log_file(line: &str) {
    static LOG_FILE: OnceLock<Option<Mutex<std::fs::File>>> = OnceLock::new();

    let log_file = LOG_FILE.get_or_init(|| {
        OpenOptions::new()
            .create(true)
            .append(true)
            .open(std::env::temp_dir().join("mila-dashboard-live.log"))
            .ok()
            .map(Mutex::new)
    });

    if let Some(log_file) = log_file {
        if let Ok(mut file) = log_file.lock() {
            let _ = writeln!(file, "{line}");
            let _ = file.flush();
        }
    }
}

fn wall_clock_timestamp() -> String {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default();
    let total_secs = now.as_secs() % 86_400;
    let hours = total_secs / 3_600;
    let minutes = (total_secs % 3_600) / 60;
    let seconds = total_secs % 60;

    format!("{hours:02}:{minutes:02}:{seconds:02}")
}
