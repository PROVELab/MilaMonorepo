#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use panic_halt as _; 

// Include bindgen definitions
#[allow(non_upper_case_globals, non_camel_case_types, non_snake_case)]
pub mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
use bindings::*;

mod sensor_helper;
use sensor_helper::*;

// --- Data Collectors ---
fn collect_battery_V(_cancel_frame_send: &mut bool) -> i32 {
    // defmt::info!("collecting battery_V");
    0
}

fn collect_motor_V(_cancel_frame_send: &mut bool) -> i32 {
    // defmt::info!("collecting motor_V");
    0
}

fn collect_prechargeState(_cancel_frame_send: &mut bool) -> i32 {
    // defmt::info!("collecting prechargeState");
    prechargeState.Off
}

fn collect_contactorState(_cancel_frame_send: &mut bool) -> i32 {
    // defmt::info!("collecting contactorState");
    prechargeState.Off
}

fn collect_prechargeLatched(_cancel_frame_send: &mut bool) -> i32 {
    // defmt::info!("collecting prechargeLatched");
    0
}


// Static array of collector callbacks
static DATA_COLLECTORS: [fn(&mut bool) -> i32; 5] = [
    collect_battery_V,
    collect_motor_V,
    collect_prechargeState,
    collect_contactorState,
    collect_prechargeLatched
];

// --- Receive Message Task ---
#[embassy_executor::task]
async fn receive_msg_task() {
    let mut plpc: core::mem::MaybeUninit<PCANListenParamsCollection> = core::mem::MaybeUninit::zeroed();
    let mut plpc = unsafe { plpc.assume_init() };

    plpc.defaultHandler = Some(defaultPacketRecv);
    plpc.size = 0;

    loop {
        unsafe {
            while waitPackets(&mut plpc) != NOT_RECEIVED {
                // Handling loop
            }
        }
        Timer::after(Duration::from_millis(10)).await;
    }
}

// --- Main Application ---
#[embassy_executor::main]
async fn main(spawner: Spawner) {
    // Initialize CAN via Pecan C-bindings
    unsafe {
        let config = pecanInit {
            nodeId: myId as i32,
            pin1: defaultPin as i32, 
            pin2: defaultPin as i32,
        };
        pecan_CanInit(config);
    }

    // Spawns background tasks for all defined numFrames 
    sensor_init(&spawner, core::ptr::null_mut(), &DATA_COLLECTORS);

    // Start CAN receive listener
    spawner.spawn(receive_msg_task()).unwrap();

    loop {
        Timer::after(Duration::from_secs(10)).await;
    }
}
