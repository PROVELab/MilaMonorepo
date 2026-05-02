mod field_trend;
mod log_buffer;
mod mcap_recorder;
mod serial_link;
mod snapshot;
mod state;
mod trend_samples;
mod types;
mod vsr_proto;

use field_trend::{build_field_trend, FieldSample, FieldTrendResponse, SampleSource};
use state::DashboardState;
use trend_samples::{collect_field_samples_from_mcap, epoch_nanos};
use types::{MotorCommandRequest, VehicleSnapshot};
use vsr_proto::dynamic_field_as_f64;

#[tauri::command]
fn get_vehicle_snapshot(state: tauri::State<DashboardState>) -> VehicleSnapshot {
    state.snapshot()
}

#[tauri::command]
fn send_motor_command(
    command: MotorCommandRequest,
    state: tauri::State<DashboardState>,
) -> Result<(), String> {
    state.send_motor_command(command)
}

#[tauri::command]
fn engage_emergency_stop(state: tauri::State<DashboardState>) -> Result<(), String> {
    state.engage_emergency_stop()
}

#[tauri::command]
async fn get_vsr_field_analysis(
    section_id: String,
    field_key: String,
    state: tauri::State<'_, DashboardState>,
) -> Result<FieldTrendResponse, String> {
    let context = state.trend_context(&section_id, &field_key)?;

    tauri::async_runtime::spawn_blocking(move || {
        let mcap_path = context.mcap_path.ok_or_else(|| {
            "MCAP stream logging is unavailable; no active .mcap recording path".to_string()
        })?;

        let mut samples = collect_field_samples_from_mcap(&mcap_path, &section_id, &field_key)?;
        if let Some(vsr) = context.latest_vsr.as_ref() {
            if let Some(value) = dynamic_field_as_f64(vsr, &section_id, &field_key) {
                samples.push(FieldSample {
                    timestamp_ns: epoch_nanos(),
                    value,
                    source: SampleSource::Live,
                });
            }
        }

        build_field_trend(section_id, field_key, context.label, context.unit, samples)
    })
    .await
    .map_err(|err| format!("field analysis task failed: {err}"))?
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let vehicle_state = DashboardState::new();
    if let Err(err) = vehicle_state.start_serial_worker() {
        log::error!("{err}");
    }

    let builder = tauri::Builder::default()
        .manage(vehicle_state)
        .invoke_handler(tauri::generate_handler![
            get_vehicle_snapshot,
            send_motor_command,
            engage_emergency_stop,
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
        });

    if let Err(err) = builder.run(tauri::generate_context!()) {
        log::error!("error while running tauri application: {err}");
    }
}
