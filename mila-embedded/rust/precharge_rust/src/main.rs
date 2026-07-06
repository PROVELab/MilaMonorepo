#![no_std]
#![no_main]
#![allow(non_snake_case)]

use panic_halt as _;
use rtt_target::{rprintln, rtt_init_print};

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
//
use embassy_stm32::rcc::{
    AHBPrescaler, APBPrescaler, Pll, PllMul, PllPDiv, PllPreDiv, PllQDiv, PllSource, Sysclk,
};
use embassy_stm32::Config;
use sensor_common::ffi::{
    addParam, combinedID, pecanInit, CANListenParam, PCANListenParamsCollection,
    MATCH_TYPE_MATCH_EXACT,
};
use sensor_common::pecan_rust::{pecan_CanInit, waitPackets};
use sensor_common::program_constants::*;

mod ffi {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
sensor_common::define_sensor_specific!();
mod vsense;
use crate::vsense::voltage_reader_task;

mod contactors;
use crate::contactors::{contactor_control_task, handle_vitals_precharge_command};

mod HBLED;
use crate::HBLED::HBLED_task;

mod imd;
use imd::imd_task;

mod sensor_main;

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    // 1. Initialize RTT (for printing)
    rtt_init_print!();

    // 2. Configure Clocks (80 MHz HSI)
    let mut config = Config::default();

    // 1. Enable Internal 16MHz clock
    config.rcc.hsi = true;

    // 2. Set PLL Source to HSI
    config.rcc.pll_src = PllSource::HSI;

    // 3. Configure PLL (16MHz / 8 * 168 / 2 = 168MHz)
    config.rcc.pll = Some(Pll {
        prediv: PllPreDiv::DIV8,
        mul: PllMul::MUL168,
        divp: Some(PllPDiv::DIV2),
        divq: Some(PllQDiv::DIV7), // 48MHz for USB
        divr: None,
    });
    config.rcc.sys = Sysclk::PLL1_P;

    // 4. Set bus prescalers
    config.rcc.ahb_pre = AHBPrescaler::DIV1; // Core at 168MHz
    config.rcc.apb1_pre = APBPrescaler::DIV4; // CAN at 42MHz (Max)
    config.rcc.apb2_pre = APBPrescaler::DIV2; //high speed peripheral bus (unused atm)

    config.enable_debug_during_sleep = true; //so ST-Link doesnt get destroyed when the stm sleeps.

    let p = embassy_stm32::init(config);

    Timer::after_millis(1000).await;
    rprintln!("*** ENTERED MAIN (Embassy) ***");

    let cfg = pecanInit {
        nodeId: prechargeID as i32,
        pin1: -1,
        pin2: -1,
    };

    pecan_CanInit(cfg, &spawner, p.CAN1, p.PA11, p.PA12).await;

    let mut listen_collection: PCANListenParamsCollection = unsafe { core::mem::zeroed() };

    sensor_main::init_sensor(&spawner, &mut listen_collection);

    //add handler for vitalsSpecific commands (for commanding the contactors state)
    unsafe {
        let listen_id = combinedID(vitalsCommand, prechargeID);
        let vitals_command = CANListenParam {
            listen_id,
            handler: Some(handle_vitals_precharge_command),
            mt: MATCH_TYPE_MATCH_EXACT,
        };
        addParam(&mut listen_collection as *mut _, vitals_command);
    }

    //spawn tasks
    spawner.spawn(HBLED_task(p.PA3)).unwrap(); 
    spawner 
        .spawn(contactor_control_task(p.PC6, p.PB12, p.PB14))
        .unwrap();
    spawner
        .spawn(voltage_reader_task(
            p.I2C1, p.PB6, p.PB7, p.DMA1_CH6, p.DMA1_CH0,
        ))
        .unwrap();
    spawner.spawn(imd_task(p.PA8, p.EXTI8, p.PC8)).unwrap();
    loop {
        // Process packets. the Timer::after is necessary cuz embassy is co-operative
        let _res = waitPackets(&mut listen_collection as *mut _);
        Timer::after(Duration::from_millis(5)).await;
    }
}
