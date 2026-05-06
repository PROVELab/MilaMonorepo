use prost_reflect::{DynamicMessage, ReflectMessage, Value};
use std::time::Duration;
use vsr::schema::VsrSubstruct;

use crate::state::VehicleInternal;
use crate::types::{DriveMode, VehicleField, VehicleSection, VehicleSnapshot};
use crate::vsr_proto::{dynamic_field_as_f32, get_field_value_string, get_nested_message};

pub type VsrSchema = Vec<(String, VsrSubstruct)>;
const VSR_STALE_TIMEOUT: Duration = Duration::from_secs(5);
const VSR_RX_RATE_WINDOW: Duration = Duration::from_secs(5);

pub fn build_snapshot(state: &VehicleInternal) -> VehicleSnapshot {
    let last_frame_age = state.last_frame_received_at.map(|t| t.elapsed());
    let is_serial_ready = last_frame_age.is_some_and(|age| age <= VSR_STALE_TIMEOUT);
    let motor_rpm = state
        .latest_vsr
        .as_ref()
        .and_then(|vsr| dynamic_field_as_f32(vsr, "motor_speed", "motor_speed"));
    let pedal_pct = state
        .latest_vsr
        .as_ref()
        .and_then(|vsr| {
            dynamic_field_as_f32(vsr, "accel_pedal", "pedal_position_pct")
                .or_else(|| dynamic_field_as_f32(vsr, "pedal", "pedal_position_pct"))
        })
        .map(|value| value.clamp(0.0, 100.0));
    let brake_pct = state
        .latest_vsr
        .as_ref()
        .and_then(|vsr| {
            dynamic_field_as_f32(vsr, "brake_pedal", "brake_position_pct")
                .or_else(|| dynamic_field_as_f32(vsr, "brake", "brake_position_pct"))
        })
        .map(|value| value.clamp(0.0, 100.0));
    let (drive_mode, cruise_target_rpm) = state
        .latest_vsr
        .as_ref()
        .map(derive_drive_mode_state)
        .unwrap_or((DriveMode::Park, None));

    let mut sections = vec![build_link_section(state)];
    if let Some(vsr) = state.latest_vsr.as_ref() {
        sections.extend(build_vsr_sections(vsr, &state.vsr_schema));
    }

    VehicleSnapshot {
        motor_rpm,
        pedal_pct,
        brake_pct,
        drive_mode,
        cruise_target_rpm,
        sections,
        live_text_logs: state.live_text_logs.iter().cloned().collect(),
        is_serial_ready,
        frames_received: state.frames_received,
        last_frame_age_seconds: last_frame_age.map(|age| age.as_secs()),
    }
}

pub fn lookup_field_meta(
    schema: &VsrSchema,
    section_id: &str,
    field_key: &str,
) -> (String, Option<String>) {
    let Some(section) = schema
        .iter()
        .find_map(|(id, section)| (id == section_id).then_some(section))
    else {
        return (field_key.to_string(), None);
    };

    let Some(field) = section.fields.get(field_key) else {
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

fn derive_drive_mode_state(vsr: &DynamicMessage) -> (DriveMode, Option<u32>) {
    let Some(drive_mode) =
        get_nested_message(vsr, "drive_mode").or_else(|| get_nested_message(vsr, "motor_command"))
    else {
        return (DriveMode::Park, None);
    };

    let Some(command_field) = drive_mode.descriptor().get_field_by_name("command") else {
        return (DriveMode::Park, None);
    };
    let command_value_binding = drive_mode.get_field(&command_field);
    let Value::Message(command_value) = command_value_binding.as_ref() else {
        return (DriveMode::Park, None);
    };

    let has_variant = |field_name: &str| {
        command_value
            .descriptor()
            .get_field_by_name(field_name)
            .is_some_and(|field| command_value.has_field(&field))
    };

    if has_variant("reverse") {
        return (DriveMode::Reverse, None);
    }
    if has_variant("park") {
        return (DriveMode::Park, None);
    }
    if has_variant("neutral") {
        return (DriveMode::Neutral, None);
    }
    if has_variant("drive") {
        return (DriveMode::Drive, None);
    }
    if let Some(cruise_field) = command_value
        .descriptor()
        .get_field_by_name("cruise_control")
    {
        if command_value.has_field(&cruise_field) {
            let target_speed_rpm = match command_value.get_field(&cruise_field).as_ref() {
                Value::Message(cruise_control) => cruise_control
                    .descriptor()
                    .get_field_by_name("target_speed_rpm")
                    .and_then(|field| match cruise_control.get_field(&field).as_ref() {
                        Value::U32(value) => Some(*value),
                        _ => None,
                    }),
                _ => None,
            };
            return (DriveMode::CruiseControl, target_speed_rpm);
        }
    }

    (DriveMode::Park, None)
}

fn build_link_section(state: &VehicleInternal) -> VehicleSection {
    let link_status = if state.serial_link_ready {
        "Connected"
    } else {
        "Waiting for serial"
    };
    let frame_age_ms = state
        .last_frame_received_at
        .map(|t| t.elapsed().as_millis().to_string())
        .unwrap_or_else(|| "n/a".to_string());
    let recent_frame_count = state
        .recent_vsr_frame_timestamps
        .iter()
        .rev()
        .take_while(|timestamp| timestamp.elapsed() <= VSR_RX_RATE_WINDOW)
        .count();
    let rolling_vsr_hz = recent_frame_count as f64 / VSR_RX_RATE_WINDOW.as_secs_f64();

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
                "frames_rx_last_5s",
                "VSR RX Frequency (last 5s)",
                format!("{rolling_vsr_hz:.2}"),
                Some("Hz"),
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
        let Some(section_message) = get_nested_message(vsr, section_key) else {
            continue;
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

        sections.push(VehicleSection::new(section_key, section_key, fields));
    }

    sections
}
