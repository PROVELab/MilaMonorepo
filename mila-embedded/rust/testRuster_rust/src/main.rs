#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_stm32::Config;
use embassy_time::Timer;
use panic_halt as _;
use rtt_target::{rprintln, rtt_init_print};

mod ffi {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[path = "../../sensor_common/src/pecan_rust.rs"]
mod pecan_rust;
#[path = "../../sensor_common/src/sensor_specific.rs"]
mod sensor_specific;
mod sensor_main;

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let config = Config::default();
    let p = embassy_stm32::init(config);
    rtt_init_print!();
    rprintln!("Starting sensor node...");

    let can_config = ffi::pecanInit {
        nodeId: ffi::myId as i32,
        pin1: -1,
        pin2: -1,
    };
    pecan_rust::pecan_CanInit(can_config, &spawner, p.CAN1, p.PA11, p.PA12).await;

    let mut plpc: ffi::PCANListenParamsCollection = unsafe { core::mem::zeroed() };
    plpc.defaultHandler = Some(ffi::defaultPacketRecv);
    plpc.size = 0;
    sensor_main::init_sensor(&spawner, &mut plpc);

    loop {
        let _ = pecan_rust::waitPackets(&mut plpc);
        Timer::after_millis(5).await;
    }
}
