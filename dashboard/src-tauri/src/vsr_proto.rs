use prost::Message;
use prost_reflect::{
    DescriptorPool, DynamicMessage, Kind, MapKey, MessageDescriptor, ReflectMessage, Value,
};
use std::sync::OnceLock;

use crate::types::{DriveMode, RequestedMotorCommand, DEFAULT_CRUISE_TARGET_RPM};

pub const VSR_MESSAGE_FULL_NAME: &str = "vsr.VehicleStatusRegister";
pub const DRIVE_MODE_MESSAGE_FULL_NAME: &str = "vsr.DriveMode";

pub fn descriptor_bytes() -> &'static [u8] {
    include_bytes!(concat!(env!("OUT_DIR"), "/vsr_descriptor.bin")).as_ref()
}

fn descriptor_pool() -> Result<&'static DescriptorPool, String> {
    static POOL: OnceLock<Result<DescriptorPool, String>> = OnceLock::new();
    match POOL.get_or_init(|| {
        DescriptorPool::decode(descriptor_bytes())
            .map_err(|err| format!("failed to decode embedded VSR descriptor set: {err}"))
    }) {
        Ok(pool) => Ok(pool),
        Err(err) => Err(err.clone()),
    }
}

fn message_descriptor(name: &str) -> Result<MessageDescriptor, String> {
    descriptor_pool()?
        .get_message_by_name(name)
        .ok_or_else(|| format!("missing message descriptor for {name}"))
}

pub fn decode_vehicle_status(payload: &[u8]) -> Result<Option<DynamicMessage>, String> {
    let descriptor = message_descriptor(VSR_MESSAGE_FULL_NAME)?;
    let vsr = DynamicMessage::decode(descriptor, payload)
        .map_err(|err| format!("failed decoding VSR payload: {err}"))?;

    if vsr.descriptor().fields().any(|field| vsr.has_field(&field)) {
        Ok(Some(vsr))
    } else {
        Ok(None)
    }
}

pub fn encode_motor_command(command: RequestedMotorCommand) -> Result<Vec<u8>, String> {
    let descriptor = message_descriptor(DRIVE_MODE_MESSAGE_FULL_NAME)?;
    let command_field = descriptor
        .get_field_by_name("command")
        .ok_or_else(|| "missing drive_mode.command field".to_string())?;
    let command_kind = command_field.kind();
    let command_value_descriptor = command_kind
        .as_message()
        .ok_or_else(|| "drive_mode.command is not a message".to_string())?;
    let mut command_value_message = DynamicMessage::new(command_value_descriptor.clone());

    let (variant_field_name, cruise_target_rpm) = match command.mode {
        DriveMode::Reverse => ("reverse", None),
        DriveMode::Park => ("park", None),
        DriveMode::Neutral => ("neutral", None),
        DriveMode::Drive => ("drive", None),
        DriveMode::CruiseControl => (
            "cruise_control",
            Some(
                command
                    .cruise_target_rpm
                    .unwrap_or(DEFAULT_CRUISE_TARGET_RPM),
            ),
        ),
    };

    let variant_field = command_value_descriptor
        .get_field_by_name(variant_field_name)
        .ok_or_else(|| format!("missing drive_mode.command.{variant_field_name} field"))?;
    let variant_kind = variant_field.kind();
    let variant_descriptor = variant_kind
        .as_message()
        .ok_or_else(|| format!("drive_mode.command.{variant_field_name} is not a message"))?;
    let mut variant_message = DynamicMessage::new(variant_descriptor.clone());

    if let Some(target_speed_rpm) = cruise_target_rpm {
        let target_field = variant_descriptor
            .get_field_by_name("target_speed_rpm")
            .ok_or_else(|| "missing cruise_control target_speed_rpm field".to_string())?;
        variant_message.set_field(&target_field, Value::U32(target_speed_rpm));
    }

    command_value_message.set_field(&variant_field, Value::Message(variant_message));

    let mut drive_mode_message = DynamicMessage::new(descriptor);
    drive_mode_message.set_field(&command_field, Value::Message(command_value_message));
    Ok(drive_mode_message.encode_to_vec())
}

pub fn get_nested_message(vsr: &DynamicMessage, section_key: &str) -> Option<DynamicMessage> {
    let section_descriptor = vsr.descriptor().get_field_by_name(section_key)?;
    if !vsr.has_field(&section_descriptor) {
        return None;
    }
    match vsr.get_field(&section_descriptor).as_ref() {
        Value::Message(message) => Some(message.clone()),
        _ => None,
    }
}

pub fn get_repeated_string_field(
    vsr: &DynamicMessage,
    section_key: &str,
    field_key: &str,
) -> Vec<String> {
    let Some(section) = get_nested_message(vsr, section_key) else {
        return Vec::new();
    };
    let Some(field_descriptor) = section.descriptor().get_field_by_name(field_key) else {
        return Vec::new();
    };

    match section.get_field(&field_descriptor).as_ref() {
        Value::List(values) => values
            .iter()
            .filter_map(|value| match value {
                Value::String(text) if !text.trim().is_empty() => Some(text.clone()),
                _ => None,
            })
            .collect(),
        _ => Vec::new(),
    }
}

pub fn dynamic_field_as_f32(
    vsr: &DynamicMessage,
    section_key: &str,
    field_key: &str,
) -> Option<f32> {
    let section = get_nested_message(vsr, section_key)?;
    let field_descriptor = section.descriptor().get_field_by_name(field_key)?;
    dynamic_value_as_f32(section.get_field(&field_descriptor).as_ref())
}

pub fn dynamic_field_as_f64(
    vsr: &DynamicMessage,
    section_key: &str,
    field_key: &str,
) -> Option<f64> {
    let section = get_nested_message(vsr, section_key)?;
    let field_descriptor = section.descriptor().get_field_by_name(field_key)?;
    dynamic_value_as_f64(section.get_field(&field_descriptor).as_ref())
}

pub fn get_field_value_string(section_message: &DynamicMessage, field_key: &str) -> Option<String> {
    let descriptor = section_message.descriptor();
    let field_descriptor = descriptor
        .get_field_by_name(field_key)
        .or_else(|| descriptor.get_field_by_json_name(field_key))?;

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
