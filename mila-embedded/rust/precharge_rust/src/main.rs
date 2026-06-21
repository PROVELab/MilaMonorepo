#![no_std]
#![no_main]
#![allow(non_snake_case)]

use panic_halt as _;
use rtt_target::{rtt_init_print, rprintln};

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
//
use embassy_stm32::rcc::{
    AHBPrescaler, APBPrescaler, Pll, PllMul, PllPDiv, PllPreDiv, PllQDiv, PllSource, Sysclk,
};
use embassy_stm32::{Config};

mod ffi {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}


use crate::ffi::{
    CANListenParam,
    PCANListenParamsCollection,
    pecanInit,
    combinedID,
    addParam,
    // Constants
    MATCH_TYPE_MATCH_EXACT,
};

#[path = "../../sensor_common/src/pecan_rust.rs"]
mod pecan_rust;
use crate::pecan_rust::{waitPackets};

mod vsense;
use crate::vsense::voltage_reader_task;

mod contactors;
use crate::contactors::{contactor_control_task, handle_vitals_precharge_command};

mod HBLED;
use crate::HBLED::HBLED_task;

#[path = "../../programConstants.rs"]
mod programConstants;
use programConstants::*;

mod imd;
use imd::imd_task;

#[path = "../../sensor_common/src/sensor_specific.rs"]
mod sensor_specific;

mod sensor_main;

#[embassy_executor::main]
async fn main(spawner: Spawner) {

    // 1. Initialize RTT
    rtt_init_print!();

    // 2. Configure Clocks (80 MHz HSI match)
    let mut config = Config::default();

   // 1. Enable Internal 16MHz clock
    config.rcc.hsi = true;

    // 2. Set PLL Source to HSI
    config.rcc.pll_src = PllSource::HSI;

    // 3. Configure PLL (16MHz / 8 * 168 / 2 = 168MHz)
    config.rcc.pll = Some(Pll {
        prediv: PllPreDiv::DIV8,
        mul: PllMul::MUL168,
        divp: Some(PllPDiv::DIV2), // Wrapped in Some()
        divq: Some(PllQDiv::DIV7), // 48MHz for USB
        divr: None,                // The "missing" field the compiler wants
    });

    // 4. Set the system clock to use the PLL
    config.rcc.sys = Sysclk::PLL1_P;

    // 5. Set bus prescalers (Note the "_pre" suffixes)
    config.rcc.ahb_pre = AHBPrescaler::DIV1;   // Core at 168MHz
    config.rcc.apb1_pre = APBPrescaler::DIV4;  // CAN at 42MHz (Max)
    config.rcc.apb2_pre = APBPrescaler::DIV2;


    config.enable_debug_during_sleep = true;    //so ST-Link doesnt get destroyed when the stm sleeps.

    let p = embassy_stm32::init(config);
        // loop{}
    Timer::after_millis(1000).await;
    rprintln!("*** ENTERED MAIN (Embassy) ***");

    let cfg = pecanInit {
        nodeId: prechargeID as i32,
        pin1: -1,
        pin2: -1,
    };
    // // We call this directly. Note: In original code this set a global NODE_ID.
    crate::pecan_rust::pecan_CanInit(cfg, &spawner, p.CAN1, p.PA11, p.PA12).await;

    let mut listen_collection: PCANListenParamsCollection = unsafe { core::mem::zeroed() };

    sensor_main::init_sensor(&spawner, &mut listen_collection);

    unsafe {
        let listen_id = combinedID(vitalsCommand, prechargeID);
        let vitals_command = CANListenParam {
            listen_id,
            handler: Some(handle_vitals_precharge_command),
            mt: MATCH_TYPE_MATCH_EXACT,
        };
        addParam(&mut listen_collection as *mut _, vitals_command);
    }

    spawner.spawn(HBLED_task(p.PA3)).unwrap();
    spawner.spawn(contactor_control_task(p.PC6, p.PB12, p.PB14)).unwrap();
    spawner.spawn(voltage_reader_task(p.I2C1, p.PB6, p.PB7, p.DMA1_CH6, p.DMA1_CH0)).unwrap();
    spawner.spawn(imd_task(p.PA8, p.EXTI8, p.PC8)).unwrap();    
    loop {
        // Process packets (This polls the queue populated by the background task)
        let _res = waitPackets(&mut listen_collection as *mut _);
        Timer::after(Duration::from_millis(5)).await;
    }
}
