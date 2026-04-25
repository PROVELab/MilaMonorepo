mod field_trend;
mod mcap_recorder;

use enumset::enum_set;
use field_trend::{build_field_trend, FieldSample, FieldTrendResponse, SampleSource};
use mcap_recorder::McapRecorder;
use prost_reflect::{
    DescriptorPool, DynamicMessage, Kind, MapKey, MessageDescriptor, ReflectMessage, Value,
};
use serde::{Deserialize, Serialize};
use serialport::{ClearBuffer, SerialPortInfo, SerialPortType};
use std::collections::VecDeque;
use std::env;
use std::fs;
use std::io::{self, Read};
use std::sync::OnceLock;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use vsr::construct_vsr::load_all_vsr_substructs;
use vsr::schema::VsrSubstruct;

const VSR_SERIAL_BAUD: u32 = 921_600;
const VSR_SERIAL_TIMEOUT: Duration = Duration::from_millis(250);
const VSR_RETRY_DELAY: Duration = Duration::from_millis(500);
const VSR_OPEN_SETTLE_DELAY: Duration = Duration::from_millis(500);
const VSR_FRAME_MAGIC_0: u8 = 0xA5;
const VSR_FRAME_MAGIC_1: u8 = 0x5A;
const VSR_FRAME_HEADER_LEN: usize = 4;
const VSR_PAYLOAD_MAX_LEN: usize = 4096;
const VSR_STREAM_BUFFER_MAX_LEN: usize = 8 * VSR_PAYLOAD_MAX_LEN;
const VSR_RESYNC_LOG_EVERY_DROPS: u64 = 500;
const VSR_NO_FRAME_WARNING_EVERY_DROPS: u64 = 50_000;
const VSR_FRAME_PROGRESS_LOG_EVERY: u64 = 100;
const MAX_LOG_LINES: usize = 120;
const VSR_MESSAGE_FULL_NAME: &str = "vsr.VehicleStatusRegister";
const MCAP_VSR_TOPIC: &str = "/vsr";

// This is only an approximate wheel-speed conversion for UI continuity.
const MOTOR_RPM_TO_MPH: f32 = 0.04;
const BATTERY_EMPTY_V: f32 = 300.0;
const BATTERY_FULL_V: f32 = 420.0;
const FALLBACK_CURRENT_LIMIT_A: f32 = 400.0;

#[derive(Clone, Copy, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub enum DriveMode {
    #[serde(rename = "P")]
    Park,
    #[serde(rename = "D")]
    Drive,
    #[serde(rename = "R")]
    Reverse,
}

impl Default for DriveMode {
    fn default() -> Self {
        Self::Park
    }
}

#[derive(Clone)]
struct VehicleState {
    inner: Arc<Mutex<VehicleInternal>>,
}

impl VehicleState {
    fn new() -> Self {
        let mut logs = VecDeque::with_capacity(MAX_LOG_LINES);
        let (vsr_schema, schema_log) = load_vsr_schema();
        push_log_line(&mut logs, schema_log);
        push_log_line(
            &mut logs,
            "dashboard backend online; waiting for MCU VSR stream",
        );

        Self {
            inner: Arc::new(Mutex::new(VehicleInternal {
                drive_mode: DriveMode::Park,
                vsr_schema,
                latest_vsr: None,
                mcap_output_path: None,
                serial_port_name: None,
                last_frame_received_at: None,
                last_frame_size_bytes: 0,
                frames_received: 0,
                decode_errors: 0,
                io_errors: 0,
                live_text_logs: logs,
            })),
        }
    }

    fn start_serial_worker(&self) {
        let shared = Arc::clone(&self.inner);
        thread::Builder::new()
            .name("mcu-vsr-reader".into())
            .spawn(move || serial_reader_main(shared))
            .expect("failed to spawn MCU VSR serial reader thread");
    }

    fn snapshot(&self) -> VehicleSnapshot {
        let guard = self.inner.lock().expect("vehicle state poisoned");
        build_snapshot(&guard)
    }

    fn set_drive_mode(&self, mode: DriveMode) -> DriveMode {
        let mut guard = self.inner.lock().expect("vehicle state poisoned");
        guard.drive_mode = mode;
        push_log_line(
            &mut guard.live_text_logs,
            format!("drive selector -> {:?}", mode),
        );
        mode
    }
}

struct VehicleInternal {
    drive_mode: DriveMode,
    vsr_schema: VsrSchema,
    latest_vsr: Option<DynamicMessage>,
    mcap_output_path: Option<String>,
    serial_port_name: Option<String>,
    last_frame_received_at: Option<Instant>,
    last_frame_size_bytes: usize,
    frames_received: u64,
    decode_errors: u64,
    io_errors: u64,
    live_text_logs: VecDeque<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct VehicleSnapshot {
    speed_mph: f32,
    torque_ratio: f32,
    battery_pct: f32,
    drive_mode: DriveMode,
    sections: Vec<VehicleSection>,
    live_text_logs: Vec<String>,
}

#[derive(Serialize)]
pub struct VehicleField {
    key: String,
    label: String,
    value: String,
    unit: Option<String>,
}

impl VehicleField {
    fn new(key: &str, label: &str, value: impl Into<String>, unit: Option<&str>) -> Self {
        Self {
            key: key.to_string(),
            label: label.to_string(),
            value: value.into(),
            unit: unit.map(ToOwned::to_owned),
        }
    }
}

#[derive(Serialize)]
pub struct VehicleSection {
    id: String,
    title: String,
    fields: Vec<VehicleField>,
}

impl VehicleSection {
    fn new(id: &str, title: &str, fields: Vec<VehicleField>) -> Self {
        Self {
            id: id.to_string(),
            title: title.to_string(),
            fields,
        }
    }
}

type VsrSchema = Vec<(String, VsrSubstruct)>;

fn load_vsr_schema() -> (VsrSchema, String) {
    match load_all_vsr_substructs() {
        Ok(schema_map) => {
            let section_count = schema_map.len();
            let field_count = schema_map
                .values()
                .map(|section| section.fields.len())
                .sum::<usize>();
            (
                schema_map.into_iter().collect(),
                format!("loaded VSR schema ({section_count} sections, {field_count} fields)"),
            )
        }
        Err(err) => (Vec::new(), format!("failed to load VSR schema: {err}")),
    }
}

fn vsr_descriptor_pool() -> &'static DescriptorPool {
    static POOL: OnceLock<DescriptorPool> = OnceLock::new();
    POOL.get_or_init(|| {
        DescriptorPool::decode(vsr_descriptor_bytes())
            .expect("failed to decode embedded VSR descriptor set")
    })
}

fn vsr_descriptor_bytes() -> &'static [u8] {
    include_bytes!(concat!(env!("OUT_DIR"), "/vsr_descriptor.bin")).as_ref()
}

fn vehicle_status_descriptor() -> MessageDescriptor {
    vsr_descriptor_pool()
        .get_message_by_name(VSR_MESSAGE_FULL_NAME)
        .expect("missing message descriptor for vsr.VehicleStatusRegister")
}

fn serial_reader_main(shared: Arc<Mutex<VehicleInternal>>) {
    let mut mcap_recorder = McapRecorder::new(VSR_MESSAGE_FULL_NAME, vsr_descriptor_bytes());
    {
        let mut state = shared.lock().expect("vehicle state poisoned");
        if let Some(warning) = mcap_recorder.take_startup_warning() {
            state.mcap_output_path = None;
            push_log_line(&mut state.live_text_logs, warning);
        } else {
            state.mcap_output_path = Some(mcap_recorder.output_path().display().to_string());
            push_log_line(
                &mut state.live_text_logs,
                format!(
                    "recording incoming VSR stream to {}",
                    mcap_recorder.output_path().display()
                ),
            );
        }
    }

    loop {
        let port_name = wait_for_serial_port_name();

        {
            let mut state = shared.lock().expect("vehicle state poisoned");
            state.serial_port_name = Some(port_name.clone());
            push_log_line(
                &mut state.live_text_logs,
                format!("connecting to MCU VSR serial port {port_name} @ {VSR_SERIAL_BAUD} baud"),
            );
        }

        let mut port = match serialport::new(&port_name, VSR_SERIAL_BAUD)
            .timeout(VSR_SERIAL_TIMEOUT)
            .open()
        {
            Ok(port) => port,
            Err(err) => {
                let mut state = shared.lock().expect("vehicle state poisoned");
                state.io_errors = state.io_errors.saturating_add(1);
                push_log_line(
                    &mut state.live_text_logs,
                    format!("failed opening {port_name}: {err}"),
                );
                drop(state);
                thread::sleep(VSR_RETRY_DELAY);
                continue;
            }
        };

        // Avoid keeping the adapter in a reset/asserted state and drop startup noise.
        let _ = port.write_data_terminal_ready(false);
        let _ = port.write_request_to_send(false);
        thread::sleep(VSR_OPEN_SETTLE_DELAY);
        let _ = port.clear(ClearBuffer::Input);

        {
            let mut state = shared.lock().expect("vehicle state poisoned");
            push_log_line(
                &mut state.live_text_logs,
                format!("MCU VSR stream connected on {port_name} (framed A5 5A + len_le)"),
            );
        }

        let mut stream_buf = Vec::<u8>::with_capacity(2 * VSR_PAYLOAD_MAX_LEN);
        let mut read_buf = [0_u8; 512];

        loop {
            let bytes_read = match port.read(&mut read_buf) {
                Ok(0) => continue,
                Ok(bytes_read) => bytes_read,
                Err(err) if err.kind() == io::ErrorKind::TimedOut => continue,
                Err(err) => {
                    let mut state = shared.lock().expect("vehicle state poisoned");
                    state.io_errors = state.io_errors.saturating_add(1);
                    push_log_line(
                        &mut state.live_text_logs,
                        format!("serial read failed on {port_name}: {err}"),
                    );
                    state.serial_port_name = None;
                    drop(state);
                    thread::sleep(VSR_RETRY_DELAY);
                    break;
                }
            };

            stream_buf.extend_from_slice(&read_buf[..bytes_read]);
            trim_stream_buffer(&mut stream_buf);

            let drain_result = drain_vsr_frames(&mut stream_buf, |payload| {
                if let Some(warning) = mcap_recorder.append_vsr(payload) {
                    let mut state = shared.lock().expect("vehicle state poisoned");
                    push_log_line(&mut state.live_text_logs, warning);
                }
            });
            if !drain_result.has_activity() {
                continue;
            }

            let mut state = shared.lock().expect("vehicle state poisoned");

            let dropped_fragments = drain_result.invalid_len_drops + drain_result.decode_drops;
            if dropped_fragments > 0 {
                let previous = state.decode_errors;
                state.decode_errors = state.decode_errors.saturating_add(dropped_fragments);
                if state.decode_errors / VSR_RESYNC_LOG_EVERY_DROPS
                    != previous / VSR_RESYNC_LOG_EVERY_DROPS
                {
                    let total_drops = state.decode_errors;
                    push_log_line(
                        &mut state.live_text_logs,
                        format!(
                            "resync drops={}, total decode/framing drops={}",
                            dropped_fragments, total_drops
                        ),
                    );
                }

                if state.frames_received == 0
                    && state.decode_errors / VSR_NO_FRAME_WARNING_EVERY_DROPS
                        != previous / VSR_NO_FRAME_WARNING_EVERY_DROPS
                {
                    push_log_line(
                        &mut state.live_text_logs,
                        "no valid VSR frames yet; check MCU firmware build, framing mode, and serial port selection",
                    );
                }
            }

            if drain_result.frames_decoded > 0 {
                let previous_frames = state.frames_received;
                state.frames_received = state
                    .frames_received
                    .saturating_add(drain_result.frames_decoded);
                state.last_frame_received_at = Some(Instant::now());
                state.last_frame_size_bytes = drain_result.last_payload_len;
                if let Some(latest_vsr) = drain_result.last_vsr {
                    state.latest_vsr = Some(latest_vsr);
                }

                if state.frames_received / VSR_FRAME_PROGRESS_LOG_EVERY
                    != previous_frames / VSR_FRAME_PROGRESS_LOG_EVERY
                {
                    let received = state.frames_received;
                    push_log_line(
                        &mut state.live_text_logs,
                        format!("MCU frames received: {received}"),
                    );
                }
            }
        }
    }
}

fn wait_for_serial_port_name() -> String {
    if let Ok(port_name) = env::var("MILA_VSR_SERIAL_PORT") {
        if !port_name.trim().is_empty() {
            return port_name;
        }
    }

    loop {
        match serialport::available_ports() {
            Ok(ports) => {
                if let Some(name) = choose_serial_port_name(&ports) {
                    return name;
                }
            }
            Err(err) => {
                log::warn!("failed enumerating serial ports: {err}");
            }
        }

        thread::sleep(VSR_RETRY_DELAY);
    }
}

fn choose_serial_port_name(ports: &[SerialPortInfo]) -> Option<String> {
    let first_matching_prefix = ports.iter().find(|port| {
        ["/dev/ttyUSB", "/dev/ttyACM", "/dev/tty.usb", "COM"]
            .iter()
            .any(|prefix| port.port_name.starts_with(prefix))
    });

    if let Some(port) = first_matching_prefix {
        return Some(port.port_name.clone());
    }

    let first_usb = ports
        .iter()
        .find(|port| matches!(port.port_type, SerialPortType::UsbPort(_)));

    first_usb
        .or_else(|| ports.first())
        .map(|port| port.port_name.clone())
}

fn trim_stream_buffer(stream_buf: &mut Vec<u8>) {
    if stream_buf.len() <= VSR_STREAM_BUFFER_MAX_LEN {
        return;
    }

    let to_drop = stream_buf.len() - VSR_STREAM_BUFFER_MAX_LEN;
    stream_buf.drain(..to_drop);
}

#[derive(Default)]
struct DrainResult {
    invalid_len_drops: u64,
    decode_drops: u64,
    frames_decoded: u64,
    last_payload_len: usize,
    last_vsr: Option<DynamicMessage>,
}

impl DrainResult {
    fn has_activity(&self) -> bool {
        self.invalid_len_drops > 0 || self.decode_drops > 0 || self.frames_decoded > 0
    }
}

fn find_magic_start(stream_buf: &[u8]) -> Option<usize> {
    stream_buf
        .windows(2)
        .position(|window| window == [VSR_FRAME_MAGIC_0, VSR_FRAME_MAGIC_1])
}

fn try_decode_vsr_payload(payload: &[u8]) -> Option<DynamicMessage> {
    DynamicMessage::decode(vehicle_status_descriptor(), payload)
        .ok()
        .filter(vsr_has_any_data)
}

fn vsr_has_any_data(vsr: &DynamicMessage) -> bool {
    let descriptor = vsr.descriptor();
    let has_any = descriptor.fields().any(|field| vsr.has_field(&field));
    has_any
}

fn drain_vsr_frames<F>(stream_buf: &mut Vec<u8>, mut on_vsr_payload: F) -> DrainResult
where
    F: FnMut(&[u8]),
{
    let mut result = DrainResult::default();
    let mut cursor = 0_usize;

    loop {
        let remaining = stream_buf.len().saturating_sub(cursor);
        if remaining < 2 {
            break;
        }

        match find_magic_start(&stream_buf[cursor..]) {
            Some(0) => {}
            Some(offset) => {
                result.invalid_len_drops = result.invalid_len_drops.saturating_add(offset as u64);
                cursor = cursor.saturating_add(offset);
                continue;
            }
            None => {
                // Keep a trailing possible first magic byte for the next read chunk.
                let keep = if stream_buf
                    .last()
                    .is_some_and(|byte| *byte == VSR_FRAME_MAGIC_0)
                {
                    1
                } else {
                    0
                };
                let to_drop = stream_buf.len().saturating_sub(cursor).saturating_sub(keep);
                result.invalid_len_drops = result.invalid_len_drops.saturating_add(to_drop as u64);
                cursor = stream_buf.len().saturating_sub(keep);
                break;
            }
        }

        let remaining = stream_buf.len().saturating_sub(cursor);
        if remaining < VSR_FRAME_HEADER_LEN {
            break;
        }

        let payload_len =
            u16::from_le_bytes([stream_buf[cursor + 2], stream_buf[cursor + 3]]) as usize;
        if payload_len == 0 || payload_len > VSR_PAYLOAD_MAX_LEN {
            // Drop one byte and continue seeking marker.
            cursor = cursor.saturating_add(1);
            result.invalid_len_drops = result.invalid_len_drops.saturating_add(1);
            continue;
        }

        let frame_len = VSR_FRAME_HEADER_LEN + payload_len;
        if remaining < frame_len {
            break;
        }

        let payload_start = cursor + VSR_FRAME_HEADER_LEN;
        let payload_end = payload_start + payload_len;
        let payload = &stream_buf[payload_start..payload_end];

        if let Some(vsr) = try_decode_vsr_payload(payload) {
            result.frames_decoded = result.frames_decoded.saturating_add(1);
            result.last_payload_len = payload_len;
            on_vsr_payload(payload);
            result.last_vsr = Some(vsr);
            cursor = payload_end;
        } else {
            // Keep the stream open and shift by one byte to seek the next possible boundary.
            cursor = cursor.saturating_add(1);
            result.decode_drops = result.decode_drops.saturating_add(1);
        }
    }

    if cursor > 0 {
        stream_buf.drain(..cursor);
    }

    result
}

fn build_snapshot(state: &VehicleInternal) -> VehicleSnapshot {
    let (speed_mph, torque_ratio, battery_pct) = state
        .latest_vsr
        .as_ref()
        .map(derive_drive_metrics)
        .unwrap_or((0.0, 0.0, 0.0));

    let mut sections = vec![build_link_section(state)];

    if let Some(vsr) = state.latest_vsr.as_ref() {
        sections.extend(build_vsr_sections(vsr, &state.vsr_schema));
    }

    VehicleSnapshot {
        speed_mph,
        torque_ratio,
        battery_pct,
        drive_mode: state.drive_mode,
        sections,
        live_text_logs: state.live_text_logs.iter().cloned().collect(),
    }
}

fn derive_drive_metrics(vsr: &DynamicMessage) -> (f32, f32, f32) {
    let motor_speed_rpm =
        dynamic_field_as_f32(vsr, "motor_speed", "motor_speed").unwrap_or_default();
    let speed_mph = motor_speed_rpm * MOTOR_RPM_TO_MPH;

    let current_reference = dynamic_field_as_f32(vsr, "motor_control", "current_reference")
        .unwrap_or_default()
        .abs();

    let current_limit = dynamic_field_as_f32(vsr, "motor_power", "motor_current_limit_arms")
        .filter(|limit| *limit > 1.0)
        .or_else(|| {
            dynamic_field_as_f32(vsr, "motor_protections_1", "dc_traction_current_limit_a")
                .filter(|limit| *limit > 1.0)
        })
        .unwrap_or(FALLBACK_CURRENT_LIMIT_A);

    let torque_ratio = (current_reference / current_limit).clamp(0.0, 1.0);

    let dc_bus_voltage =
        dynamic_field_as_f32(vsr, "motor_power", "measured_dc_voltage_v").unwrap_or_default();

    let battery_pct = if dc_bus_voltage <= 0.0 {
        0.0
    } else {
        ((dc_bus_voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0)
            .clamp(0.0, 100.0)
    };

    (speed_mph, torque_ratio, battery_pct)
}

fn build_link_section(state: &VehicleInternal) -> VehicleSection {
    let link_status = if state.last_frame_received_at.is_some() {
        "Connected"
    } else {
        "Waiting for frames"
    };

    let frame_age_ms = state
        .last_frame_received_at
        .map(|t| t.elapsed().as_millis().to_string())
        .unwrap_or_else(|| "n/a".to_string());

    VehicleSection::new(
        "mcu-link",
        "MCU Link",
        vec![
            VehicleField::new(
                "serial_port",
                "Serial Port",
                state.serial_port_name.as_deref().unwrap_or("not selected"),
                None,
            ),
            VehicleField::new("status", "Status", link_status, None),
            VehicleField::new(
                "frames_rx",
                "Frames RX",
                state.frames_received.to_string(),
                None,
            ),
            VehicleField::new(
                "last_frame_size",
                "Last Frame Size",
                state.last_frame_size_bytes.to_string(),
                Some("B"),
            ),
            VehicleField::new("last_frame_age", "Last Frame Age", frame_age_ms, Some("ms")),
            VehicleField::new(
                "decode_errors",
                "Decode Errors",
                state.decode_errors.to_string(),
                None,
            ),
            VehicleField::new("io_errors", "I/O Errors", state.io_errors.to_string(), None),
        ],
    )
}

fn build_vsr_sections(vsr: &DynamicMessage, schema: &VsrSchema) -> Vec<VehicleSection> {
    let mut sections = Vec::with_capacity(schema.len());

    for (section_key, section_schema) in schema {
        let section_message = match get_nested_message(vsr, section_key) {
            Some(section_message) => section_message,
            _ => continue,
        };

        let mut fields = Vec::with_capacity(section_schema.fields.len());
        for (field_key, field_schema) in &section_schema.fields {
            let label = if field_schema.desc.trim().is_empty() {
                field_key.to_string()
            } else {
                field_schema.desc.trim().to_string()
            };
            let value = get_field_value_string(&section_message, field_key)
                .unwrap_or_else(|| "n/a".to_string());
            let unit = if field_schema.unit.trim().is_empty() {
                None
            } else {
                Some(field_schema.unit.as_str())
            };
            fields.push(VehicleField::new(field_key, &label, value, unit));
        }

        let section_id = section_key.clone();
        let title = section_key.clone();
        sections.push(VehicleSection::new(&section_id, &title, fields));
    }

    sections
}

fn dynamic_field_as_f32(vsr: &DynamicMessage, section_key: &str, field_key: &str) -> Option<f32> {
    let section = get_nested_message(vsr, section_key)?;
    let field_descriptor = section.descriptor().get_field_by_name(field_key)?;
    dynamic_value_as_f32(section.get_field(&field_descriptor).as_ref())
}

fn dynamic_field_as_f64(vsr: &DynamicMessage, section_key: &str, field_key: &str) -> Option<f64> {
    let section = get_nested_message(vsr, section_key)?;
    let field_descriptor = section.descriptor().get_field_by_name(field_key)?;
    dynamic_value_as_f64(section.get_field(&field_descriptor).as_ref())
}

fn get_nested_message(vsr: &DynamicMessage, section_key: &str) -> Option<DynamicMessage> {
    let section_descriptor = vsr.descriptor().get_field_by_name(section_key)?;
    if !vsr.has_field(&section_descriptor) {
        return None;
    }
    match vsr.get_field(&section_descriptor).as_ref() {
        Value::Message(message) => Some(message.clone()),
        _ => None,
    }
}

fn get_field_value_string(section_message: &DynamicMessage, field_key: &str) -> Option<String> {
    let descriptor = section_message.descriptor();
    let field_descriptor = descriptor
        .get_field_by_name(field_key)
        .or_else(|| descriptor.get_field_by_json_name(field_key))?;

    // For fields with true presence semantics (messages/oneof/optional),
    // treat unset as missing. For scalar proto3 fields, use default values.
    if field_descriptor.supports_presence() && !section_message.has_field(&field_descriptor) {
        return None;
    }
    let value = section_message.get_field(&field_descriptor);
    Some(format_dynamic_value(
        value.as_ref(),
        &field_descriptor.kind(),
    ))
}

fn format_dynamic_value(value: &Value, kind: &Kind) -> String {
    match value {
        Value::Bool(v) => bool_to_on_off(*v).to_string(),
        Value::I32(v) => v.to_string(),
        Value::I64(v) => v.to_string(),
        Value::U32(v) => v.to_string(),
        Value::U64(v) => v.to_string(),
        Value::F32(v) => format!("{v:.2}"),
        Value::F64(v) => format!("{v:.2}"),
        Value::String(text) => text.clone(),
        Value::Bytes(bytes) => format!("{bytes:?}"),
        Value::EnumNumber(number) => match kind {
            Kind::Enum(enum_descriptor) => enum_descriptor
                .get_value(*number)
                .map(|value_descriptor| value_descriptor.name().to_string())
                .unwrap_or_else(|| number.to_string()),
            _ => number.to_string(),
        },
        Value::Message(message) => format_dynamic_message(message),
        Value::List(values) => {
            let rendered = values
                .iter()
                .map(|entry| format_dynamic_value(entry, &Kind::String))
                .collect::<Vec<_>>();
            format!("[{}]", rendered.join(", "))
        }
        Value::Map(entries) => {
            let rendered = entries
                .iter()
                .map(|(entry_key, entry_value)| {
                    format!(
                        "{}={}",
                        format_map_key(entry_key),
                        format_dynamic_value(entry_value, &Kind::String)
                    )
                })
                .collect::<Vec<_>>();
            format!("{{{}}}", rendered.join(", "))
        }
    }
}

fn format_dynamic_message(message: &DynamicMessage) -> String {
    let descriptor = message.descriptor();
    let rendered = message
        .descriptor()
        .fields()
        .filter(|field| message.has_field(field))
        .map(|field| {
            let value = message.get_field(&field);
            match value.as_ref() {
                // For empty marker payloads (common for oneof/unit variants), emit just the field name.
                Value::Message(child) if child.descriptor().fields().len() == 0 => {
                    field.name().to_string()
                }
                _ => format!(
                    "{}={}",
                    field.name(),
                    format_dynamic_value(value.as_ref(), &field.kind())
                ),
            }
        })
        .collect::<Vec<_>>();

    if rendered.is_empty() {
        if descriptor.oneofs().len() > 0 {
            "unset".to_string()
        } else {
            "set".to_string()
        }
    } else {
        rendered.join(", ")
    }
}

fn dynamic_value_as_f32(value: &Value) -> Option<f32> {
    match value {
        Value::I32(v) => Some(*v as f32),
        Value::I64(v) => Some(*v as f32),
        Value::U32(v) => Some(*v as f32),
        Value::U64(v) => Some(*v as f32),
        Value::F32(v) => Some(*v),
        Value::F64(v) => Some(*v as f32),
        Value::EnumNumber(v) => Some(*v as f32),
        _ => None,
    }
}

fn dynamic_value_as_f64(value: &Value) -> Option<f64> {
    match value {
        Value::I32(v) => Some(*v as f64),
        Value::I64(v) => Some(*v as f64),
        Value::U32(v) => Some(*v as f64),
        Value::U64(v) => Some(*v as f64),
        Value::F32(v) => Some(*v as f64),
        Value::F64(v) => Some(*v),
        Value::EnumNumber(v) => Some(*v as f64),
        _ => None,
    }
}

fn format_map_key(key: &MapKey) -> String {
    match key {
        MapKey::Bool(v) => v.to_string(),
        MapKey::I32(v) => v.to_string(),
        MapKey::I64(v) => v.to_string(),
        MapKey::U32(v) => v.to_string(),
        MapKey::U64(v) => v.to_string(),
        MapKey::String(v) => v.clone(),
    }
}

fn bool_to_on_off(value: bool) -> &'static str {
    if value {
        "On"
    } else {
        "Off"
    }
}

fn lookup_field_meta(
    schema: &VsrSchema,
    section_id: &str,
    field_key: &str,
) -> (String, Option<String>) {
    let section = schema.iter().find_map(|(id, section)| {
        if id == section_id {
            Some(section)
        } else {
            None
        }
    });

    let Some(section) = section else {
        return (field_key.to_string(), None);
    };

    let field = section.fields.get(field_key);
    let Some(field) = field else {
        return (field_key.to_string(), None);
    };

    let label = if field.desc.trim().is_empty() {
        field_key.to_string()
    } else {
        field.desc.trim().to_string()
    };
    let unit = if field.unit.trim().is_empty() {
        None
    } else {
        Some(field.unit.trim().to_string())
    };

    (label, unit)
}

fn collect_field_samples_from_mcap(
    mcap_path: &str,
    section_id: &str,
    field_key: &str,
) -> Result<Vec<FieldSample>, String> {
    let file_bytes =
        fs::read(mcap_path).map_err(|err| format!("failed reading {mcap_path}: {err}"))?;
    let stream = mcap::MessageStream::new_with_options(
        file_bytes.as_slice(),
        enum_set!(mcap::read::Options::IgnoreEndMagic),
    )
    .map_err(|err| format!("failed opening mcap stream {mcap_path}: {err}"))?;

    let mut samples = Vec::new();
    for message in stream {
        let message = match message {
            Ok(message) => message,
            Err(_tail_err) => break,
        };
        if message.channel.topic != MCAP_VSR_TOPIC {
            continue;
        }

        let Some(vsr) = try_decode_vsr_payload(message.data.as_ref()) else {
            continue;
        };
        let Some(value) = dynamic_field_as_f64(&vsr, section_id, field_key) else {
            continue;
        };

        samples.push(FieldSample {
            timestamp_ns: message.log_time,
            value,
            source: SampleSource::Mcap,
        });
    }

    Ok(samples)
}

fn epoch_nanos() -> u64 {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    u64::try_from(nanos).unwrap_or(u64::MAX)
}

fn push_log_line(logs: &mut VecDeque<String>, message: impl AsRef<str>) {
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

#[tauri::command]
fn get_vehicle_snapshot(state: tauri::State<VehicleState>) -> VehicleSnapshot {
    state.snapshot()
}

#[tauri::command]
fn set_drive_mode(mode: DriveMode, state: tauri::State<VehicleState>) -> DriveMode {
    state.set_drive_mode(mode)
}

#[tauri::command]
async fn get_vsr_field_analysis(
    section_id: String,
    field_key: String,
    state: tauri::State<'_, VehicleState>,
) -> Result<FieldTrendResponse, String> {
    let (mcap_path, latest_vsr, label, unit) = {
        let guard = state.inner.lock().expect("vehicle state poisoned");
        let (label, unit) = lookup_field_meta(&guard.vsr_schema, &section_id, &field_key);
        (
            guard.mcap_output_path.clone(),
            guard.latest_vsr.clone(),
            label,
            unit,
        )
    };

    tauri::async_runtime::spawn_blocking(move || {
        let mcap_path = mcap_path.ok_or_else(|| {
            "MCAP stream logging is unavailable; no active .mcap recording path".to_string()
        })?;

        let mut samples = collect_field_samples_from_mcap(&mcap_path, &section_id, &field_key)?;
        if let Some(vsr) = latest_vsr.as_ref() {
            if let Some(value) = dynamic_field_as_f64(vsr, &section_id, &field_key) {
                samples.push(FieldSample {
                    timestamp_ns: epoch_nanos(),
                    value,
                    source: SampleSource::Live,
                });
            }
        }

        build_field_trend(section_id, field_key, label, unit, samples)
    })
    .await
    .map_err(|err| format!("field analysis task failed: {err}"))?
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let vehicle_state = VehicleState::new();
    vehicle_state.start_serial_worker();

    tauri::Builder::default()
        .manage(vehicle_state)
        .invoke_handler(tauri::generate_handler![
            get_vehicle_snapshot,
            set_drive_mode,
            get_vsr_field_analysis
        ])
        .setup(|app| {
            if cfg!(debug_assertions) {
                app.handle().plugin(
                    tauri_plugin_log::Builder::default()
                        .level(log::LevelFilter::Info)
                        .build(),
                )?;
            }
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
