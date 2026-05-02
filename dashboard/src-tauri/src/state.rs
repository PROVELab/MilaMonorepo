use prost_reflect::DynamicMessage;
use std::collections::VecDeque;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::Instant;
use vsr::construct_vsr::load_all_vsr_substructs;

use crate::log_buffer::{push_log_line, MAX_LOG_LINES};
use crate::serial_link;
use crate::snapshot::{build_snapshot, lookup_field_meta, VsrSchema};
use crate::types::{
    DriveMode, MotorCommandRequest, OutboundCommand, RequestedMotorCommand, VehicleSnapshot,
};

pub type SharedVehicleState = Arc<Mutex<VehicleInternal>>;

#[derive(Clone)]
pub struct DashboardState {
    pub inner: SharedVehicleState,
    outbound_tx: mpsc::Sender<OutboundCommand>,
}

pub struct VehicleInternal {
    pub vsr_schema: VsrSchema,
    pub latest_vsr: Option<DynamicMessage>,
    pub mcap_output_path: Option<String>,
    pub serial_port_name: Option<String>,
    pub serial_link_ready: bool,
    pub last_frame_received_at: Option<Instant>,
    pub last_frame_size_bytes: usize,
    pub frames_received: u64,
    pub decode_errors: u64,
    pub io_errors: u64,
    pub emergency_stop_latched: bool,
    pub outbound_rx: Option<mpsc::Receiver<OutboundCommand>>,
    pub live_text_logs: VecDeque<String>,
}

pub struct TrendContext {
    pub mcap_path: Option<String>,
    pub latest_vsr: Option<DynamicMessage>,
    pub label: String,
    pub unit: Option<String>,
}

impl DashboardState {
    pub fn new() -> Self {
        let mut logs = VecDeque::with_capacity(MAX_LOG_LINES);
        let vsr_schema = match load_all_vsr_substructs() {
            Ok(schema_map) => {
                let section_count = schema_map.len();
                let field_count = schema_map
                    .values()
                    .map(|section| section.fields.len())
                    .sum::<usize>();
                push_log_line(
                    &mut logs,
                    format!("loaded VSR schema ({section_count} sections, {field_count} fields)"),
                );
                schema_map.into_iter().collect()
            }
            Err(err) => {
                push_log_line(&mut logs, format!("failed to load VSR schema: {err}"));
                Vec::new()
            }
        };
        push_log_line(
            &mut logs,
            "dashboard backend online; waiting for MCU VSR stream",
        );
        let (outbound_tx, outbound_rx) = mpsc::channel();

        Self {
            inner: Arc::new(Mutex::new(VehicleInternal {
                vsr_schema,
                latest_vsr: None,
                mcap_output_path: None,
                serial_port_name: None,
                serial_link_ready: false,
                last_frame_received_at: None,
                last_frame_size_bytes: 0,
                frames_received: 0,
                decode_errors: 0,
                io_errors: 0,
                emergency_stop_latched: false,
                outbound_rx: Some(outbound_rx),
                live_text_logs: logs,
            })),
            outbound_tx,
        }
    }

    pub fn start_serial_worker(&self) -> Result<(), String> {
        let outbound_rx = {
            let mut guard = self
                .inner
                .lock()
                .map_err(|_| "vehicle state lock is poisoned".to_string())?;
            guard
                .outbound_rx
                .take()
                .ok_or_else(|| "serial worker already started".to_string())?
        };

        serial_link::spawn_serial_worker(Arc::clone(&self.inner), outbound_rx)
    }

    pub fn snapshot(&self) -> VehicleSnapshot {
        match self.inner.lock() {
            Ok(guard) => build_snapshot(&guard),
            Err(_) => VehicleSnapshot::empty(vec![
                "vehicle state lock is poisoned; telemetry unavailable".to_string(),
            ]),
        }
    }

    pub fn send_motor_command(&self, request: MotorCommandRequest) -> Result<(), String> {
        let command = RequestedMotorCommand {
            mode: request.mode,
            cruise_target_rpm: request.cruise_target_rpm,
        };
        if command.mode == DriveMode::CruiseControl && command.cruise_target_rpm.is_none() {
            return Err("cruise-control commands must include cruiseTargetRpm".to_string());
        }

        {
            let mut guard = self
                .inner
                .lock()
                .map_err(|_| "vehicle state lock is poisoned".to_string())?;
            if !guard.serial_link_ready {
                push_log_line(
                    &mut guard.live_text_logs,
                    format!(
                        "dropped motor command (serial link not ready): {:?}",
                        command
                    ),
                );
                return Ok(());
            }
        }

        self.outbound_tx
            .send(OutboundCommand::Motor(command))
            .map_err(|err| format!("failed to queue outbound motor command: {err}"))?;

        if let Ok(mut guard) = self.inner.lock() {
            push_log_line(
                &mut guard.live_text_logs,
                format!("queued motor command for serial write: {:?}", command),
            );
        }
        Ok(())
    }

    pub fn engage_emergency_stop(&self) -> Result<(), String> {
        let mut guard = self
            .inner
            .lock()
            .map_err(|_| "vehicle state lock is poisoned".to_string())?;
        if guard.emergency_stop_latched {
            push_log_line(
                &mut guard.live_text_logs,
                "emergency stop already latched; ignoring duplicate trigger",
            );
            return Ok(());
        }
        if !guard.serial_link_ready {
            push_log_line(
                &mut guard.live_text_logs,
                "WARNING: emergency stop requested while serial link not ready; dropping command",
            );
            return Ok(());
        }
        guard.emergency_stop_latched = true;
        push_log_line(
            &mut guard.live_text_logs,
            "WARNING: emergency stop requested (latched)",
        );
        drop(guard);

        self.outbound_tx
            .send(OutboundCommand::EmergencyStop)
            .map_err(|err| format!("failed to queue emergency stop command: {err}"))
    }

    pub fn trend_context(&self, section_id: &str, field_key: &str) -> Result<TrendContext, String> {
        let guard = self
            .inner
            .lock()
            .map_err(|_| "vehicle state lock is poisoned".to_string())?;
        let (label, unit) = lookup_field_meta(&guard.vsr_schema, section_id, field_key);

        Ok(TrendContext {
            mcap_path: guard.mcap_output_path.clone(),
            latest_vsr: guard.latest_vsr.clone(),
            label,
            unit,
        })
    }
}
