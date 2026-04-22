use prost::Message;
use serde::{Deserialize, Serialize};
use serialport::{SerialPortInfo, SerialPortType};
use std::collections::VecDeque;
use std::env;
use std::io;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

mod vsr_proto {
    include!(concat!(env!("OUT_DIR"), "/vsr.rs"));
}

use vsr_proto::VehicleStatusRegister;

const VSR_SERIAL_BAUD: u32 = 921_600;
const VSR_SERIAL_TIMEOUT: Duration = Duration::from_millis(250);
const VSR_RETRY_DELAY: Duration = Duration::from_millis(500);
const VSR_FRAME_HEADER_LEN: usize = 2;
const VSR_PAYLOAD_MAX_LEN: usize = 4096;
const MAX_LOG_LINES: usize = 120;

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
        push_log_line(
            &mut logs,
            "dashboard backend online; waiting for MCU VSR stream",
        );

        Self {
            inner: Arc::new(Mutex::new(VehicleInternal {
                drive_mode: DriveMode::Park,
                latest_vsr: None,
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
    latest_vsr: Option<VehicleStatusRegister>,
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
    label: String,
    value: String,
    unit: Option<String>,
}

impl VehicleField {
    fn new(label: &str, value: impl Into<String>, unit: Option<&str>) -> Self {
        Self {
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

fn serial_reader_main(shared: Arc<Mutex<VehicleInternal>>) {
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

        {
            let mut state = shared.lock().expect("vehicle state poisoned");
            push_log_line(
                &mut state.live_text_logs,
                format!("MCU VSR stream connected on {port_name}"),
            );
        }

        let mut payload_buf = Vec::with_capacity(512);
        loop {
            match read_single_framed_payload(&mut *port, &mut payload_buf) {
                Ok(Some(payload_len)) => {
                    match VehicleStatusRegister::decode(payload_buf[..payload_len].as_ref()) {
                        Ok(vsr) => {
                            let mut state = shared.lock().expect("vehicle state poisoned");
                            state.latest_vsr = Some(vsr);
                            state.frames_received = state.frames_received.saturating_add(1);
                            state.last_frame_received_at = Some(Instant::now());
                            state.last_frame_size_bytes = payload_len;

                            if state.frames_received % 100 == 0 {
                                let received = state.frames_received;
                                push_log_line(
                                    &mut state.live_text_logs,
                                    format!("MCU frames received: {received}"),
                                );
                            }
                        }
                        Err(err) => {
                            let mut state = shared.lock().expect("vehicle state poisoned");
                            state.decode_errors = state.decode_errors.saturating_add(1);
                            if state.decode_errors % 10 == 1 {
                                let decode_errors = state.decode_errors;
                                push_log_line(
                                    &mut state.live_text_logs,
                                    format!(
                                        "protobuf decode error (count={}): {err}",
                                        decode_errors
                                    ),
                                );
                            }
                        }
                    }
                }
                Ok(None) => {}
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

fn read_single_framed_payload(
    port: &mut dyn serialport::SerialPort,
    payload_buf: &mut Vec<u8>,
) -> io::Result<Option<usize>> {
    let mut header = [0_u8; VSR_FRAME_HEADER_LEN];
    match port.read_exact(&mut header) {
        Ok(()) => {}
        Err(err) if err.kind() == io::ErrorKind::TimedOut => {
            return Ok(None);
        }
        Err(err) => return Err(err),
    }

    let payload_len = u16::from_le_bytes(header) as usize;
    if payload_len == 0 || payload_len > VSR_PAYLOAD_MAX_LEN {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            format!("invalid MCU VSR frame length: {payload_len}"),
        ));
    }

    payload_buf.resize(payload_len, 0);
    port.read_exact(payload_buf)?;
    Ok(Some(payload_len))
}

fn build_snapshot(state: &VehicleInternal) -> VehicleSnapshot {
    let (speed_mph, torque_ratio, battery_pct) = state
        .latest_vsr
        .as_ref()
        .map(derive_drive_metrics)
        .unwrap_or((0.0, 0.0, 0.0));

    let mut sections = vec![build_link_section(state)];

    if let Some(vsr) = state.latest_vsr.as_ref() {
        sections.extend(build_vsr_sections(vsr));
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

fn derive_drive_metrics(vsr: &VehicleStatusRegister) -> (f32, f32, f32) {
    let motor_speed_rpm = vsr
        .motor_speed
        .as_ref()
        .map(|speed| speed.motor_speed as f32)
        .unwrap_or_default();
    let speed_mph = motor_speed_rpm * MOTOR_RPM_TO_MPH;

    let current_reference = vsr
        .motor_control
        .as_ref()
        .map(|control| control.current_reference as f32)
        .unwrap_or_default()
        .abs();

    let current_limit = vsr
        .motor_power
        .as_ref()
        .map(|power| power.motor_current_limit_arms)
        .filter(|limit| *limit > 1.0)
        .or_else(|| {
            vsr.motor_protections_1
                .as_ref()
                .map(|prot| prot.dc_traction_current_limit_a as f32)
                .filter(|limit| *limit > 1.0)
        })
        .unwrap_or(FALLBACK_CURRENT_LIMIT_A);

    let torque_ratio = (current_reference / current_limit).clamp(0.0, 1.0);

    let dc_bus_voltage = vsr
        .motor_power
        .as_ref()
        .map(|power| power.measured_dc_voltage_v)
        .unwrap_or_default();

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
                "Serial Port",
                state.serial_port_name.as_deref().unwrap_or("not selected"),
                None,
            ),
            VehicleField::new("Status", link_status, None),
            VehicleField::new("Frames RX", state.frames_received.to_string(), None),
            VehicleField::new(
                "Last Frame Size",
                state.last_frame_size_bytes.to_string(),
                Some("B"),
            ),
            VehicleField::new("Last Frame Age", frame_age_ms, Some("ms")),
            VehicleField::new("Decode Errors", state.decode_errors.to_string(), None),
            VehicleField::new("I/O Errors", state.io_errors.to_string(), None),
        ],
    )
}

fn build_vsr_sections(vsr: &VehicleStatusRegister) -> Vec<VehicleSection> {
    let mut sections = Vec::new();

    if let Some(command) = vsr.motor_command.as_ref() {
        let (command_name, cruise_target) = decode_motor_command(command);
        let mut fields = vec![VehicleField::new("Command", command_name, None)];
        if let Some(target) = cruise_target {
            fields.push(VehicleField::new(
                "Cruise Target",
                target.to_string(),
                Some("rpm"),
            ));
        }

        sections.push(VehicleSection::new(
            "motor-command",
            "Motor Command",
            fields,
        ));
    }

    if let Some(control) = vsr.motor_control.as_ref() {
        sections.push(VehicleSection::new(
            "motor-control",
            "Motor Control",
            vec![
                VehicleField::new(
                    "Current Reference",
                    control.current_reference.to_string(),
                    Some("Arms"),
                ),
                VehicleField::new(
                    "Discharge Limit",
                    control.discharge_limit_pct.to_string(),
                    Some("%"),
                ),
                VehicleField::new(
                    "Charge Limit",
                    control.charge_limit_pct.to_string(),
                    Some("%"),
                ),
            ],
        ));
    }

    if let Some(speed) = vsr.motor_speed.as_ref() {
        sections.push(VehicleSection::new(
            "motor-speed",
            "Motor Speed",
            vec![
                VehicleField::new("Motor Speed", speed.motor_speed.to_string(), Some("rpm")),
                VehicleField::new(
                    "Quadrature Current",
                    format_float(speed.quadrature_current),
                    Some("Arms"),
                ),
                VehicleField::new(
                    "Direct Current",
                    format_float(speed.direct_current),
                    Some("Arms"),
                ),
            ],
        ));
    }

    if let Some(power) = vsr.motor_power.as_ref() {
        sections.push(VehicleSection::new(
            "motor-power",
            "Motor Power",
            vec![
                VehicleField::new(
                    "Measured DC Voltage",
                    format_float(power.measured_dc_voltage_v),
                    Some("V"),
                ),
                VehicleField::new(
                    "Calculated DC Current",
                    format_float(power.calculated_dc_current_a),
                    Some("A"),
                ),
                VehicleField::new(
                    "Current Limit",
                    format_float(power.motor_current_limit_arms),
                    Some("A"),
                ),
            ],
        ));
    }

    if let Some(error_state) = vsr.motor_error_state.as_ref() {
        sections.push(VehicleSection::new(
            "motor-error-state",
            "Motor Error State",
            vec![VehicleField::new(
                "Motor State",
                motor_state_label(error_state.motor_state),
                None,
            )],
        ));
    }

    if let Some(safety) = vsr.motor_safety.as_ref() {
        sections.push(VehicleSection::new(
            "motor-safety",
            "Motor Safety",
            vec![
                VehicleField::new("Protection Code", safety.protection_code.to_string(), None),
                VehicleField::new(
                    "Safety Error Code",
                    safety.safety_error_code.to_string(),
                    None,
                ),
                VehicleField::new("Motor Temp", safety.motor_temp.to_string(), Some("F")),
                VehicleField::new(
                    "Inverter Bridge Temp",
                    safety.inverter_bridge_temp.to_string(),
                    Some("F"),
                ),
                VehicleField::new("Bus Cap Temp", safety.bus_cap_temp.to_string(), Some("F")),
                VehicleField::new("PWM Status", safety.pwm_status.to_string(), None),
            ],
        ));
    }

    if let Some(prot1) = vsr.motor_protections_1.as_ref() {
        sections.push(VehicleSection::new(
            "motor-protections-1",
            "Motor Protections 1",
            vec![
                VehicleField::new("CAN Timeout", prot1.can_timeout_ms.to_string(), Some("ms")),
                VehicleField::new(
                    "DC Regen Current Limit",
                    prot1.dc_regen_current_limit_neg_a.to_string(),
                    Some("A"),
                ),
                VehicleField::new(
                    "DC Traction Current Limit",
                    prot1.dc_traction_current_limit_a.to_string(),
                    Some("A"),
                ),
                VehicleField::new(
                    "Overspeed Protection",
                    prot1.overspeed_protection_speed_rpm.to_string(),
                    Some("rpm x10"),
                ),
                VehicleField::new(
                    "Stall Protection Current",
                    prot1.stall_protection_current_a.to_string(),
                    Some("A"),
                ),
                VehicleField::new(
                    "Stall Protection Time",
                    prot1.stall_protection_time_ms.to_string(),
                    Some("ms"),
                ),
                VehicleField::new(
                    "Stall Protection Type",
                    prot1.stall_protection_type.to_string(),
                    None,
                ),
            ],
        ));
    }

    if let Some(prot2) = vsr.motor_protections_2.as_ref() {
        sections.push(VehicleSection::new(
            "motor-protections-2",
            "Motor Protections 2",
            vec![
                VehicleField::new(
                    "Max Motor Temp",
                    prot2.max_motor_temp_c.to_string(),
                    Some("C"),
                ),
                VehicleField::new(
                    "Motor Temp High Gain",
                    prot2.motor_temp_high_gain_a_per_c.to_string(),
                    Some("A/C"),
                ),
                VehicleField::new(
                    "Max Inverter Temp",
                    prot2.max_inverter_temp_c.to_string(),
                    Some("C"),
                ),
                VehicleField::new(
                    "Inverter Temp High Gain",
                    prot2.inverter_temp_high_gain_a_per_c.to_string(),
                    Some("A/C"),
                ),
                VehicleField::new(
                    "Id Overcurrent Limit",
                    prot2.id_overcurrent_limit_a.to_string(),
                    Some("A"),
                ),
                VehicleField::new(
                    "Overvoltage Limit",
                    prot2.overvoltage_limit_v.to_string(),
                    Some("V"),
                ),
                VehicleField::new(
                    "Shutdown Voltage Limit",
                    prot2.shutdown_voltage_limit_v.to_string(),
                    Some("V"),
                ),
            ],
        ));
    }

    if let Some(pedal) = vsr.pedal.as_ref() {
        sections.push(VehicleSection::new(
            "pedal",
            "Pedal",
            vec![
                VehicleField::new(
                    "Pedal Position",
                    format_float(pedal.pedal_position_pct),
                    Some("%"),
                ),
                VehicleField::new("Pedal Raw 1", format_float(pedal.pedal_raw_1), None),
                VehicleField::new("Pedal Raw 2", format_float(pedal.pedal_raw_2), None),
                VehicleField::new(
                    "Pedal Supply Voltage",
                    format_float(pedal.pedal_supply_voltage),
                    Some("mV"),
                ),
                VehicleField::new("TX Value", pedal.tx_value.to_string(), None),
                VehicleField::new("Use Pedal", bool_to_on_off(pedal.use_pedal), None),
            ],
        ));
    }

    sections
}

fn decode_motor_command(command: &vsr_proto::MotorCommand) -> (String, Option<u32>) {
    let kind = command
        .command
        .as_ref()
        .and_then(|value| value.kind.as_ref());

    match kind {
        Some(vsr_proto::motor_command::command_value::Kind::Idle(_)) => ("idle".to_string(), None),
        Some(vsr_proto::motor_command::command_value::Kind::Pedal(_)) => {
            ("pedal".to_string(), None)
        }
        Some(vsr_proto::motor_command::command_value::Kind::Cruise(cruise)) => {
            ("cruise".to_string(), Some(cruise.target_speed_rpm))
        }
        None => ("unset".to_string(), None),
    }
}

fn motor_state_label(raw_state: i32) -> String {
    use vsr_proto::motor_error_state::MotorState;

    match MotorState::try_from(raw_state) {
        Ok(MotorState::MotorOk) => "MOTOR_OK".to_string(),
        Ok(MotorState::MotorErrorStop) => "MOTOR_ERROR_STOP".to_string(),
        Err(_) => format!("UNKNOWN({raw_state})"),
    }
}

fn format_float(value: f32) -> String {
    format!("{value:.2}")
}

fn bool_to_on_off(value: bool) -> &'static str {
    if value {
        "On"
    } else {
        "Off"
    }
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

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let vehicle_state = VehicleState::new();
    vehicle_state.start_serial_worker();

    tauri::Builder::default()
        .manage(vehicle_state)
        .invoke_handler(tauri::generate_handler![
            get_vehicle_snapshot,
            set_drive_mode
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
