use serialport::{ClearBuffer, SerialPortInfo, SerialPortType};
use std::env;
use std::io::{self, Read};
use std::path::Path;
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};

use crate::log_buffer::push_log_line;
use crate::mcap_recorder::McapRecorder;
use crate::state::SharedVehicleState;
use crate::types::{DriveMode, OutboundCommand, RequestedMotorCommand};
use crate::vsr_proto::{
    decode_vehicle_status, descriptor_bytes, encode_motor_command, get_repeated_string_field,
    VSR_MESSAGE_FULL_NAME,
};

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
const VSR_FRAME_PROGRESS_LOG_EVERY: u64 = 2_000;
const VSR_RX_RATE_WINDOW: Duration = Duration::from_secs(5);
const VSR_RX_RATE_MAX_SAMPLES: usize = 512;

pub fn spawn_serial_worker(
    shared: SharedVehicleState,
    outbound_rx: mpsc::Receiver<OutboundCommand>,
) -> Result<(), String> {
    thread::Builder::new()
        .name("mcu-vsr-serial".into())
        .spawn(move || serial_worker_main(shared, outbound_rx))
        .map(|_| ())
        .map_err(|err| format!("failed to spawn MCU serial worker: {err}"))
}

fn serial_worker_main(shared: SharedVehicleState, outbound_rx: mpsc::Receiver<OutboundCommand>) {
    let mut mcap_recorder = McapRecorder::new(VSR_MESSAGE_FULL_NAME, descriptor_bytes());
    with_state(&shared, |state| {
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
    });

    loop {
        let port_name = wait_for_serial_port_name();
        mark_connecting(&shared, &port_name);

        let mut port = match serialport::new(&port_name, VSR_SERIAL_BAUD)
            .timeout(VSR_SERIAL_TIMEOUT)
            .open()
        {
            Ok(port) => port,
            Err(err) => {
                with_state(&shared, |state| {
                    state.io_errors = state.io_errors.saturating_add(1);
                    state.serial_port_name = None;
                    state.serial_link_ready = false;
                    push_log_line(
                        &mut state.live_text_logs,
                        format!("failed opening {port_name}: {err}"),
                    );
                });
                thread::sleep(VSR_RETRY_DELAY);
                continue;
            }
        };

        let _ = port.write_data_terminal_ready(false);
        let _ = port.write_request_to_send(false);
        thread::sleep(VSR_OPEN_SETTLE_DELAY);
        let _ = port.clear(ClearBuffer::Input);
        mark_connected(&shared, &port_name);

        let mut stream_buf = Vec::<u8>::with_capacity(2 * VSR_PAYLOAD_MAX_LEN);
        let mut read_buf = [0_u8; 512];

        loop {
            if !process_outbound_commands(&shared, &outbound_rx, &mut *port) {
                reset_serial_link(&shared);
                drop_pending_outbound_commands(&shared, &outbound_rx);
                break;
            }

            let bytes_read = match port.read(&mut read_buf) {
                Ok(0) => continue,
                Ok(bytes_read) => bytes_read,
                Err(err) if err.kind() == io::ErrorKind::TimedOut => continue,
                Err(err) => {
                    with_state(&shared, |state| {
                        state.io_errors = state.io_errors.saturating_add(1);
                        state.serial_port_name = None;
                        state.serial_link_ready = false;
                        state.recent_vsr_frame_timestamps.clear();
                        push_log_line(
                            &mut state.live_text_logs,
                            format!("serial read failed on {port_name}: {err}"),
                        );
                    });
                    drop_pending_outbound_commands(&shared, &outbound_rx);
                    thread::sleep(VSR_RETRY_DELAY);
                    break;
                }
            };

            stream_buf.extend_from_slice(&read_buf[..bytes_read]);
            trim_stream_buffer(&mut stream_buf);

            let drain_result = drain_vsr_frames(&mut stream_buf, |payload, vsr| {
                if let Some(warning) = mcap_recorder.append_vsr(payload) {
                    with_state(&shared, |state| {
                        push_log_line(&mut state.live_text_logs, warning);
                    });
                }

                let mcu_logs = get_repeated_string_field(vsr, "log", "message");
                if !mcu_logs.is_empty() {
                    with_state(&shared, |state| {
                        for line in mcu_logs {
                            push_log_line(&mut state.live_text_logs, format!("MCU: {line}"));
                        }
                    });
                }
            });
            if drain_result.has_activity() {
                apply_drain_result(&shared, drain_result);
            }
        }
    }
}

fn mark_connecting(shared: &SharedVehicleState, port_name: &str) {
    with_state(shared, |state| {
        state.serial_port_name = Some(port_name.to_string());
        state.serial_link_ready = false;
        push_log_line(
            &mut state.live_text_logs,
            format!("connecting to MCU VSR serial port {port_name} @ {VSR_SERIAL_BAUD} baud"),
        );
    });
}

fn mark_connected(shared: &SharedVehicleState, port_name: &str) {
    with_state(shared, |state| {
        state.serial_link_ready = true;
        push_log_line(
            &mut state.live_text_logs,
            format!("MCU VSR stream connected on {port_name} (framed A5 5A + len_le)"),
        );
    });
}

fn reset_serial_link(shared: &SharedVehicleState) {
    with_state(shared, |state| {
        state.serial_port_name = None;
        state.serial_link_ready = false;
        state.recent_vsr_frame_timestamps.clear();
    });
}

fn process_outbound_commands(
    shared: &SharedVehicleState,
    outbound_rx: &mpsc::Receiver<OutboundCommand>,
    port: &mut dyn serialport::SerialPort,
) -> bool {
    loop {
        let command = match outbound_rx.try_recv() {
            Ok(command) => command,
            Err(mpsc::TryRecvError::Empty) => break,
            Err(mpsc::TryRecvError::Disconnected) => break,
        };

        let payload = match encode_outbound_command_payload(&command) {
            Ok(payload) => payload,
            Err(err) => {
                with_state(shared, |state| {
                    push_log_line(
                        &mut state.live_text_logs,
                        format!("failed encoding outbound command: {err}"),
                    );
                });
                continue;
            }
        };
        let frame = match frame_payload(&payload) {
            Ok(frame) => frame,
            Err(err) => {
                with_state(shared, |state| {
                    push_log_line(
                        &mut state.live_text_logs,
                        format!("failed framing outbound command: {err}"),
                    );
                });
                continue;
            }
        };

        if let Err(err) = write_all_serial(port, &frame) {
            with_state(shared, |state| {
                state.io_errors = state.io_errors.saturating_add(1);
                push_log_line(
                    &mut state.live_text_logs,
                    format!("serial write failed: {err}"),
                );
            });
            return false;
        }

        with_state(shared, |state| match command {
            OutboundCommand::Motor(command) => {
                push_log_line(
                    &mut state.live_text_logs,
                    format!(
                        "sent motor command -> {:?} (payload={}B frame={}B)",
                        command,
                        payload.len(),
                        frame.len()
                    ),
                );
            }
            OutboundCommand::EmergencyStop => {
                push_log_line(
                    &mut state.live_text_logs,
                    "sent emergency stop command (mapped to PARK for now)",
                );
            }
        });
    }

    true
}

fn encode_outbound_command_payload(command: &OutboundCommand) -> Result<Vec<u8>, String> {
    match command {
        OutboundCommand::Motor(requested) => encode_motor_command(*requested),
        OutboundCommand::EmergencyStop => encode_motor_command(RequestedMotorCommand {
            mode: DriveMode::Park,
            cruise_target_rpm: None,
        }),
    }
}

fn write_all_serial(port: &mut dyn serialport::SerialPort, data: &[u8]) -> io::Result<()> {
    let mut offset = 0;
    while offset < data.len() {
        let written = port.write(&data[offset..])?;
        if written == 0 {
            return Err(io::Error::new(
                io::ErrorKind::WriteZero,
                "serial write returned 0 bytes",
            ));
        }
        offset += written;
    }
    Ok(())
}

fn drop_pending_outbound_commands(
    shared: &SharedVehicleState,
    outbound_rx: &mpsc::Receiver<OutboundCommand>,
) {
    let mut dropped = 0_u64;
    while outbound_rx.try_recv().is_ok() {
        dropped = dropped.saturating_add(1);
    }

    if dropped > 0 {
        with_state(shared, |state| {
            push_log_line(
                &mut state.live_text_logs,
                format!("dropped {dropped} queued outbound command(s) after serial disconnect"),
            );
        });
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
                // SITL serial port:
                let p = Path::new("/dev/pts/1");
                if let Some(name) = choose_serial_port_name(&ports) {
                    return name;
                } else if p.exists() {
                    return p.to_str().unwrap().to_string(); // this is basically guaranteed0
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
    if let Some(port) = ports.iter().find(|port| {
        ["/dev/ttyUSB", "/dev/ttyACM", "/dev/tty.usb", "COM"]
            .iter()
            .any(|prefix| port.port_name.starts_with(prefix))
    }) {
        return Some(port.port_name.clone());
    }

    ports
        .iter()
        .find(|port| matches!(port.port_type, SerialPortType::UsbPort(_)))
        .map(|port| port.port_name.clone())
}

fn frame_payload(payload: &[u8]) -> Result<Vec<u8>, String> {
    if payload.is_empty() {
        return Err("payload must not be empty".to_string());
    }
    if payload.len() > VSR_PAYLOAD_MAX_LEN {
        return Err(format!(
            "payload too large: {} (max {})",
            payload.len(),
            VSR_PAYLOAD_MAX_LEN
        ));
    }
    if payload.len() > u16::MAX as usize {
        return Err("payload too large for u16 framing length".to_string());
    }

    let payload_len = payload.len() as u16;
    let mut framed = Vec::with_capacity(VSR_FRAME_HEADER_LEN + payload.len());
    framed.push(VSR_FRAME_MAGIC_0);
    framed.push(VSR_FRAME_MAGIC_1);
    framed.extend_from_slice(&payload_len.to_le_bytes());
    framed.extend_from_slice(payload);
    Ok(framed)
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
    last_vsr: Option<prost_reflect::DynamicMessage>,
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

fn drain_vsr_frames<F>(stream_buf: &mut Vec<u8>, mut on_vsr_payload: F) -> DrainResult
where
    F: FnMut(&[u8], &prost_reflect::DynamicMessage),
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

        match decode_vehicle_status(payload) {
            Ok(Some(vsr)) => {
                result.frames_decoded = result.frames_decoded.saturating_add(1);
                result.last_payload_len = payload_len;
                on_vsr_payload(payload, &vsr);
                result.last_vsr = Some(vsr);
                cursor = payload_end;
            }
            Ok(None) | Err(_) => {
                cursor = cursor.saturating_add(1);
                result.decode_drops = result.decode_drops.saturating_add(1);
            }
        }
    }

    if cursor > 0 {
        stream_buf.drain(..cursor);
    }

    result
}

fn apply_drain_result(shared: &SharedVehicleState, drain_result: DrainResult) {
    with_state(shared, |state| {
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
            let now = Instant::now();
            state.frames_received = state
                .frames_received
                .saturating_add(drain_result.frames_decoded);
            state.last_frame_received_at = Some(now);
            state.last_frame_size_bytes = drain_result.last_payload_len;
            if let Some(latest_vsr) = drain_result.last_vsr {
                state.latest_vsr = Some(latest_vsr);
            }

            for _ in 0..drain_result.frames_decoded {
                state.recent_vsr_frame_timestamps.push_back(now);
            }

            let cutoff = now.checked_sub(VSR_RX_RATE_WINDOW).unwrap_or(now);
            while state
                .recent_vsr_frame_timestamps
                .front()
                .is_some_and(|timestamp| *timestamp < cutoff)
            {
                state.recent_vsr_frame_timestamps.pop_front();
            }
            while state.recent_vsr_frame_timestamps.len() > VSR_RX_RATE_MAX_SAMPLES {
                state.recent_vsr_frame_timestamps.pop_front();
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
    });
}

fn with_state(shared: &SharedVehicleState, f: impl FnOnce(&mut crate::state::VehicleInternal)) {
    if let Ok(mut state) = shared.lock() {
        f(&mut state);
    }
}
