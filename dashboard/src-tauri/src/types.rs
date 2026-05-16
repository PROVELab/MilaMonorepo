use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Default, Deserialize, Serialize, PartialEq, Eq)]
pub enum DriveMode {
    #[serde(rename = "Reverse")]
    Reverse,
    #[default]
    #[serde(rename = "Park")]
    Park,
    #[serde(rename = "Neutral")]
    Neutral,
    #[serde(rename = "Drive")]
    Drive,
    #[serde(rename = "Cruise Control")]
    CruiseControl,
}

#[derive(Clone, Copy, Debug)]
pub struct RequestedMotorCommand {
    pub mode: DriveMode,
    pub cruise_target_rpm: Option<u32>,
}

#[derive(Debug)]
pub enum OutboundCommand {
    Motor(RequestedMotorCommand),
    EmergencyStop,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct MotorCommandRequest {
    pub mode: DriveMode,
    pub cruise_target_rpm: Option<u32>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct VehicleSnapshot {
    pub motor_rpm: Option<f32>,
    pub pedal_pct: Option<f32>,
    pub brake_pct: Option<f32>,
    pub drive_mode: DriveMode,
    pub cruise_target_rpm: Option<u32>,
    pub sections: Vec<VehicleSection>,
    pub live_text_logs: Vec<String>,
    pub is_serial_ready: bool,
    pub frames_received: u64,
    pub last_frame_age_seconds: Option<u64>,
}

impl VehicleSnapshot {
    pub fn empty(logs: Vec<String>) -> Self {
        Self {
            motor_rpm: None,
            pedal_pct: None,
            brake_pct: None,
            drive_mode: DriveMode::Park,
            cruise_target_rpm: None,
            sections: Vec::new(),
            live_text_logs: logs,
            is_serial_ready: false,
            frames_received: 0,
            last_frame_age_seconds: None,
        }
    }
}

#[derive(Serialize)]
pub struct VehicleField {
    pub key: String,
    pub label: String,
    pub value: String,
    pub unit: Option<String>,
}

impl VehicleField {
    pub fn new(key: &str, label: &str, value: impl Into<String>, unit: Option<&str>) -> Self {
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
    pub id: String,
    pub title: String,
    pub fields: Vec<VehicleField>,
}

impl VehicleSection {
    pub fn new(id: &str, title: &str, fields: Vec<VehicleField>) -> Self {
        Self {
            id: id.to_string(),
            title: title.to_string(),
            fields,
        }
    }
}
