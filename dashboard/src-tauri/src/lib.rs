use serde::{Deserialize, Serialize};
use std::collections::VecDeque;
use std::sync::Mutex;
use std::time::{Duration, Instant};

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
    DriveMode::Park
  }
}

#[derive(Default)]
struct VehicleState {
  inner: Mutex<VehicleInternal>,
}

impl VehicleState {
  fn snapshot(&self) -> VehicleSnapshot {
    let mut guard = self.inner.lock().expect("vehicle state poisoned");
    guard.snapshot()
  }

  fn set_drive_mode(&self, mode: DriveMode) -> DriveMode {
    let mut guard = self.inner.lock().expect("vehicle state poisoned");
    guard.drive_mode = mode;
    guard.push_log(format!("drive -> {:?}", mode));
    mode
  }
}

struct VehicleInternal {
  drive_mode: DriveMode,
  start_time: Instant,
  last_log_emit: Instant,
  logs: VecDeque<String>,
}

impl Default for VehicleInternal {
  fn default() -> Self {
    let mut logs = VecDeque::with_capacity(80);
    logs.push_front("telemetry online".into());
    Self {
      drive_mode: DriveMode::Park,
      start_time: Instant::now(),
      last_log_emit: Instant::now(),
      logs,
    }
  }
}

impl VehicleInternal {
  fn push_log(&mut self, entry: String) {
    if self.logs.len() >= 80 {
      self.logs.pop_back();
    }
    self.logs.push_front(entry);
  }

  fn snapshot(&mut self) -> VehicleSnapshot {
    let elapsed = self.start_time.elapsed().as_secs_f32();
    let signed_speed = match self.drive_mode {
      DriveMode::Drive => 32.0 + (elapsed * 0.7).sin() * 6.0,
      DriveMode::Reverse => -5.0 + (elapsed * 0.9).sin(),
      DriveMode::Park => (elapsed * 0.1).sin() * 0.4,
    };

    let torque = match self.drive_mode {
      DriveMode::Park => 0.0,
      _ => 0.35 + 0.25 * (elapsed * 0.8).sin().abs(),
    };

    let battery = 79.0 + (elapsed * 0.05).sin() * 2.5;
    let imu = ImuReading {
      pitch: (elapsed * 0.4).sin() * 7.0,
      roll: (elapsed * 0.3).cos() * 5.0,
      yaw: (elapsed * 12.0) % 360.0,
    };

    if self.last_log_emit.elapsed() > Duration::from_millis(900) {
      self.last_log_emit = Instant::now();
      self.push_log(format!(
        "{:?}: speed={:.1} torque={:.0}% battery={:.1}",
        self.drive_mode,
        signed_speed,
        torque * 100.0,
        battery
      ));
    }

    VehicleSnapshot {
      speed_mph: signed_speed,
      torque_ratio: torque.clamp(0.0, 1.0),
      battery_pct: battery,
      drive_mode: self.drive_mode,
      imu,
      sections: build_sections(self.drive_mode, signed_speed, torque, battery, elapsed),
      live_text_logs: self.logs.iter().cloned().collect(),
      derived_metrics: build_metrics(elapsed, torque, battery),
    }
  }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct VehicleSnapshot {
  speed_mph: f32,
  torque_ratio: f32,
  battery_pct: f32,
  drive_mode: DriveMode,
  imu: ImuReading,
  sections: Vec<VehicleSection>,
  live_text_logs: Vec<String>,
  derived_metrics: Vec<DerivedMetric>,
}

#[derive(Serialize)]
pub struct ImuReading {
  pitch: f32,
  roll: f32,
  yaw: f32,
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
      label: label.into(),
      value: value.into(),
      unit: unit.map(|u| u.to_string()),
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
      id: id.into(),
      title: title.into(),
      fields,
    }
  }
}

#[derive(Serialize)]
pub struct MetricWindow {
  label: String,
  value: String,
}

impl MetricWindow {
  fn new(label: &str, value: impl Into<String>) -> Self {
    Self {
      label: label.into(),
      value: value.into(),
    }
  }
}

#[derive(Serialize)]
pub struct DerivedMetric {
  label: String,
  unit: String,
  current: String,
  windows: Vec<MetricWindow>,
  sparkline: Vec<f32>,
}

impl DerivedMetric {
  fn new(
    label: &str,
    unit: &str,
    current: impl Into<String>,
    windows: Vec<MetricWindow>,
    sparkline: Vec<f32>,
  ) -> Self {
    Self {
      label: label.into(),
      unit: unit.into(),
      current: current.into(),
      windows,
      sparkline,
    }
  }
}

fn build_sections(
  mode: DriveMode,
  speed: f32,
  torque: f32,
  battery: f32,
  elapsed: f32,
) -> Vec<VehicleSection> {
  let pack_voltage = 780.0 + (elapsed * 0.25).sin() * 14.0;
  let pack_current = torque * 420.0;
  let coolant_in = 32.0 + (elapsed * 0.3).sin() * 3.0;
  let coolant_out = coolant_in + 5.0 + (elapsed * 0.6).sin();
  let module_delta = 1.5 + (elapsed * 0.2).sin();
  let hv_isolation = 4.8 + (elapsed * 0.5).sin() * 0.2;
  let inverter_temp = 48.0 + (elapsed * 0.5).sin() * 6.0;
  let gearbox_temp = 80.0 + (elapsed * 0.25).sin() * 4.0;
  let suspension = 110.0 + (elapsed * 0.9).sin() * 5.0;

  vec![
    VehicleSection::new(
      "battery-pack",
      "Battery Pack",
      vec![
        VehicleField::new("State of Charge", format!("{battery:.1}"), Some("%")),
        VehicleField::new("Pack Voltage", format!("{pack_voltage:.0}"), Some("V")),
        VehicleField::new("Pack Current", format!("{pack_current:.0}"), Some("A")),
        VehicleField::new("Module Δ", format!("{module_delta:.2}"), Some("°C")),
        VehicleField::new("Coolant Inlet", format!("{coolant_in:.1}"), Some("°C")),
        VehicleField::new("Coolant Outlet", format!("{coolant_out:.1}"), Some("°C")),
        VehicleField::new(
          "Heater Duty",
          format!("{:.0}", (elapsed * 0.4).sin() * 15.0 + 20.0),
          Some("%"),
        ),
        VehicleField::new("Isolation", format!("{hv_isolation:.2}"), Some("kΩ")),
        VehicleField::new(
          "Contactor State",
          if torque > 0.05 { "Closed" } else { "Open" },
          None,
        ),
        VehicleField::new(
          "Charge Rate",
          format!("{:.0}", (elapsed * 0.3).sin().max(0.0) * 150.0),
          Some("kW"),
        ),
      ],
    ),
    VehicleSection::new(
      "powertrain",
      "Powertrain",
      vec![
        VehicleField::new("Vehicle Speed", format!("{speed:.1}"), Some("mph")),
        VehicleField::new("Front Torque", format!("{:.0}", torque * 320.0), Some("Nm")),
        VehicleField::new("Rear Torque", format!("{:.0}", torque * 360.0), Some("Nm")),
        VehicleField::new("Inverter Temp", format!("{inverter_temp:.1}"), Some("°C")),
        VehicleField::new(
          "DC/DC Load",
          format!("{:.2}", (elapsed * 0.2).sin() * 0.8 + 1.6),
          Some("kW"),
        ),
        VehicleField::new("Regen Limit", format!("{:.0}", torque * 210.0), Some("Nm")),
        VehicleField::new("Gearbox Temp", format!("{gearbox_temp:.1}"), Some("°C")),
        VehicleField::new(
          "Clutch Slip",
          format!("{:.2}", (elapsed * 0.8).sin().abs() * 1.8),
          Some("%"),
        ),
        VehicleField::new("Drive Mode", format!("{mode:?}"), None),
      ],
    ),
    VehicleSection::new(
      "chassis",
      "Chassis & Dynamics",
      vec![
        VehicleField::new("Yaw Rate", format!("{:.2}", (elapsed * 0.9).sin() * 6.0), Some("°/s")),
        VehicleField::new("Pitch Rate", format!("{:.2}", (elapsed * 0.7).cos() * 3.0), Some("°/s")),
        VehicleField::new("Roll Rate", format!("{:.2}", (elapsed * 0.4).sin() * 2.0), Some("°/s")),
        VehicleField::new(
          "Suspension FL",
          format!("{:.0}", suspension + elapsed.sin()),
          Some("mm"),
        ),
        VehicleField::new(
          "Suspension FR",
          format!("{:.0}", suspension + elapsed.cos()),
          Some("mm"),
        ),
        VehicleField::new(
          "Suspension RL",
          format!("{:.0}", suspension + (elapsed * 1.3).sin()),
          Some("mm"),
        ),
        VehicleField::new(
          "Suspension RR",
          format!("{:.0}", suspension + (elapsed * 1.1).cos()),
          Some("mm"),
        ),
        VehicleField::new(
          "Brake Temp",
          format!("{:.0}", 220.0 + elapsed.sin().abs() * 40.0),
          Some("°C"),
        ),
        VehicleField::new(
          "Steering Angle",
          format!("{:.1}", (elapsed * 0.6).sin() * 14.0),
          Some("°"),
        ),
      ],
    ),
    VehicleSection::new(
      "thermal",
      "Thermal Systems",
      vec![
        VehicleField::new(
          "Cabin Setpoint",
          format!("{:.1}", 20.0 + (elapsed * 0.1).sin()),
          Some("°C"),
        ),
        VehicleField::new(
          "Cabin Temp",
          format!("{:.1}", 19.5 + (elapsed * 0.05).sin()),
          Some("°C"),
        ),
        VehicleField::new(
          "Heat Pump Mode",
          if torque > 0.2 { "Cooling" } else { "Heating" },
          None,
        ),
        VehicleField::new(
          "Radiator Fan",
          format!("{:.0}", (elapsed * 0.9).sin() * 30.0 + 60.0),
          Some("%"),
        ),
        VehicleField::new("Battery Heater", if battery < 65.0 { "On" } else { "Off" }, None),
        VehicleField::new(
          "Compressor Speed",
          format!("{:.0}", (elapsed * 0.7).sin().abs() * 7000.0),
          Some("rpm"),
        ),
        VehicleField::new(
          "Ambient",
          format!("{:.1}", 12.0 + (elapsed * 0.02).sin() * 6.0),
          Some("°C"),
        ),
      ],
    ),
  ]
}

fn build_metrics(elapsed: f32, torque: f32, battery: f32) -> Vec<DerivedMetric> {
  let make_sparkline = |base: f32, variance: f32| -> Vec<f32> {
    (0..30)
      .map(|idx| base + ((elapsed * 0.3) + (idx as f32) * 0.25).sin() * variance)
      .collect()
  };

  let average_slice = |values: &[f32], count: usize| -> f32 {
    if values.is_empty() {
      return 0.0;
    }
    let start = values.len().saturating_sub(count);
    let slice = &values[start..];
    slice.iter().copied().sum::<f32>() / slice.len() as f32
  };

  let windows = |spark: &[f32], suffix: &str| -> Vec<MetricWindow> {
    vec![
      MetricWindow::new("5m", format!("{:.1}{suffix}", average_slice(spark, 5))),
      MetricWindow::new("10m", format!("{:.1}{suffix}", average_slice(spark, 10))),
      MetricWindow::new("30m", format!("{:.1}{suffix}", average_slice(spark, 20))),
    ]
  };

  let energy_spark = make_sparkline(26.0 + torque * 8.0, 2.4);
  let regen_spark = make_sparkline(68.0 + torque * 20.0, 4.0);
  let thermal_spark = make_sparkline(12.0 + (elapsed * 0.1).sin() * 2.0, 1.2);
  let efficiency_spark = make_sparkline(3.6 - torque, 0.2);

  let energy_windows = windows(&energy_spark, "");
  let regen_windows = windows(&regen_spark, "%");
  let thermal_windows = windows(&thermal_spark, "");
  let efficiency_windows = windows(&efficiency_spark, "");
  let health_windows = windows(&energy_spark, "%");

  vec![
    DerivedMetric::new(
      "Energy Consumption",
      "kWh / 100 mi",
      format!("{:.1}", energy_spark.last().copied().unwrap_or_default()),
      energy_windows,
      energy_spark,
    ),
    DerivedMetric::new(
      "Regen Efficiency",
      "%",
      format!("{:.1}", regen_spark.last().copied().unwrap_or_default()),
      regen_windows,
      regen_spark,
    ),
    DerivedMetric::new(
      "Thermal Load",
      "kW",
      format!("{:.1}", thermal_spark.last().copied().unwrap_or_default()),
      thermal_windows,
      thermal_spark,
    ),
    DerivedMetric::new(
      "Trip Efficiency",
      "mi / kWh",
      format!("{:.2}", efficiency_spark.last().copied().unwrap_or(3.2).max(2.3)),
      efficiency_windows,
      efficiency_spark,
    ),
    DerivedMetric::new(
      "Battery Health Index",
      "%",
      format!("{:.1}", (battery / 100.0 * 98.0).clamp(80.0, 99.0)),
      health_windows,
      make_sparkline(95.0, 0.4),
    ),
  ]
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
  tauri::Builder::default()
    .manage(VehicleState::default())
    .invoke_handler(tauri::generate_handler![get_vehicle_snapshot, set_drive_mode])
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
