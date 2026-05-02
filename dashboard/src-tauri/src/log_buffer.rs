use std::collections::VecDeque;
use std::time::{SystemTime, UNIX_EPOCH};

pub const MAX_LOG_LINES: usize = 120;

pub fn push_log_line(logs: &mut VecDeque<String>, message: impl AsRef<str>) {
    if logs.len() >= MAX_LOG_LINES {
        logs.pop_back();
    }

    let timestamp = wall_clock_timestamp();
    logs.push_front(format!("[{timestamp}] {}", message.as_ref()));
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
