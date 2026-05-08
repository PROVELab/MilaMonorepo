use mcap::records::MessageHeader;
use mcap::{Compression, WriteOptions, Writer};
use std::collections::BTreeMap;
use std::fs::File;
use std::io::BufWriter;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const MCAP_TOPIC: &str = "/vsr";
const MCAP_SCHEMA_ENCODING: &str = "protobuf";
const MCAP_MESSAGE_ENCODING: &str = "protobuf";
const MCAP_CHUNK_SIZE_BYTES: u64 = 4 * 1024 * 1024;
// Compile-time hard cap; tune as needed.
const MCAP_HARD_FILE_SIZE_LIMIT_BYTES: u64 = 1024 * 1024 * 1024;
const MCAP_FLUSH_EVERY_MESSAGES: u32 = 50;
const MCAP_FLUSH_INTERVAL: Duration = Duration::from_secs(1);

pub struct McapRecorder {
    writer: Option<Writer<BufWriter<File>>>,
    channel_id: u16,
    sequence: u32,
    messages_since_flush: u32,
    last_flush_at: Instant,
    output_path: PathBuf,
    startup_warning: Option<String>,
}

impl McapRecorder {
    pub fn new(schema_name: &str, schema_data: &[u8]) -> Self {
        let output_path = PathBuf::from(format!("{}.mcap", epoch_millis()));

        let init_result = (|| -> Result<(Writer<BufWriter<File>>, u16), String> {
            let file = File::create(&output_path)
                .map_err(|err| format!("failed creating {}: {err}", output_path.display()))?;

            let mut writer = WriteOptions::new()
                .compression(Some(Compression::Zstd))
                .chunk_size(Some(MCAP_CHUNK_SIZE_BYTES))
                .disable_seeking(true)
                .create(BufWriter::new(file))
                .map_err(|err| format!("failed creating MCAP writer: {err}"))?;

            let schema_id = writer
                .add_schema(schema_name, MCAP_SCHEMA_ENCODING, schema_data)
                .map_err(|err| format!("failed adding MCAP schema: {err}"))?;

            let channel_id = writer
                .add_channel(
                    schema_id,
                    MCAP_TOPIC,
                    MCAP_MESSAGE_ENCODING,
                    &BTreeMap::new(),
                )
                .map_err(|err| format!("failed adding MCAP channel: {err}"))?;

            writer
                .flush()
                .map_err(|err| format!("failed flushing MCAP header chunk: {err}"))?;

            Ok((writer, channel_id))
        })();

        let mut recorder = match init_result {
            Ok((writer, channel_id)) => Self {
                writer: Some(writer),
                channel_id,
                sequence: 0,
                messages_since_flush: 0,
                last_flush_at: Instant::now(),
                output_path,
                startup_warning: None,
            },
            Err(err) => Self {
                writer: None,
                channel_id: 0,
                sequence: 0,
                messages_since_flush: 0,
                last_flush_at: Instant::now(),
                output_path,
                startup_warning: Some(format!("MCAP logging disabled: {err}")),
            },
        };

        if let Some(warning) = recorder.enforce_hard_size_limit() {
            recorder.startup_warning = Some(warning);
        }

        recorder
    }

    pub fn output_path(&self) -> &Path {
        &self.output_path
    }

    pub fn take_startup_warning(&mut self) -> Option<String> {
        self.startup_warning.take()
    }

    pub fn append_vsr(&mut self, payload: &[u8]) -> Option<String> {
        let writer = self.writer.as_mut()?;

        let log_time = epoch_nanos();
        let header = MessageHeader {
            channel_id: self.channel_id,
            sequence: self.sequence,
            log_time,
            publish_time: log_time,
        };

        let write_result = writer.write_to_known_channel(&header, payload);
        if let Err(err) = write_result {
            return Some(
                self.disable_logging(format!("MCAP logging stopped: write failed: {err}")),
            );
        }

        self.sequence = self.sequence.wrapping_add(1);
        self.messages_since_flush = self.messages_since_flush.saturating_add(1);

        if self.messages_since_flush >= MCAP_FLUSH_EVERY_MESSAGES
            || self.last_flush_at.elapsed() >= MCAP_FLUSH_INTERVAL
        {
            let writer = self.writer.as_mut()?;

            let flush_result = writer.flush();
            if let Err(err) = flush_result {
                return Some(
                    self.disable_logging(format!("MCAP logging stopped: flush failed: {err}")),
                );
            }

            self.messages_since_flush = 0;
            self.last_flush_at = Instant::now();

            if let Some(warning) = self.enforce_hard_size_limit() {
                return Some(warning);
            }
        }

        None
    }

    fn enforce_hard_size_limit(&mut self) -> Option<String> {
        self.writer.as_ref()?;

        let file_size = match std::fs::metadata(&self.output_path) {
            Ok(metadata) => metadata.len(),
            Err(err) => {
                return Some(self.disable_logging(format!(
                    "MCAP logging stopped: failed to read file size for {}: {err}",
                    self.output_path.display()
                )));
            }
        };

        if file_size < MCAP_HARD_FILE_SIZE_LIMIT_BYTES {
            return None;
        }

        Some(self.disable_logging(format!(
            "MCAP logging stopped: reached hard size limit {} bytes (current {} bytes) for {}",
            MCAP_HARD_FILE_SIZE_LIMIT_BYTES,
            file_size,
            self.output_path.display()
        )))
    }

    fn disable_logging(&mut self, warning: String) -> String {
        self.writer = None;
        self.messages_since_flush = 0;
        self.last_flush_at = Instant::now();
        warning
    }
}

fn epoch_nanos() -> u64 {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    u64::try_from(nanos).unwrap_or(u64::MAX)
}

fn epoch_millis() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
}
