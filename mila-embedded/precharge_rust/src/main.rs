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
    CANPacket,
    CANListenParam,
    PCANListenParamsCollection,
    pecanInit,
    MAX_SIZE_PACKET_DATA,
    combinedID,
    addParam,
    sendStatusUpdate,
    // Constants
    MATCH_TYPE_MATCH_EXACT,
};


mod hw;
use crate::hw::{sendPacket, waitPackets};

mod vsense;
use crate::vsense::voltage_reader_task;

mod contactors;
use crate::contactors::{contactor_control_task, contactorsOn, contactorsOff};

mod HBLED;
use crate::HBLED::HBLED_task;

mod programConstants;
use programConstants::*;

mod imd;
use imd::imd_task;

#[no_mangle]
pub extern "C" fn respond_to_hb(_p: *mut CANPacket) -> i16 {

    let mut pkt = CANPacket {
        data: [0; MAX_SIZE_PACKET_DATA as usize],
        id: 0,
        dataSize: 0,
        rtr: true,
        extendedID: false,
    };

    unsafe {
        pkt.id = combinedID(HBPong, prechargeID) as i32;
    }
    rprintln!("sending HBPong");

    sendPacket(&mut pkt as *mut CANPacket); 
    return 0;
}


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

    // HeartBeat LED
    pecanInit {nodeId: prechargeID as i32,  pin1: -1, pin2: -1};


    //*******************************8 */
    let cfg = pecanInit {
        nodeId: 1,
        pin1: -1,
        pin2: -1,
    };
    // // We call this directly. Note: In original code this set a global NODE_ID.
    // crate::hw::pecan_CanInit(cfg, &spawner, p.CAN1, p.PA11, p.PA12).await;

    // let mut listen_collection: PCANListenParamsCollection = unsafe { core::mem::zeroed() };

    // unsafe {
    //     let listen_id = combinedID(HBPing, vitalsID);
    //     let baby_duck = CANListenParam {
    //         listen_id,
    //         handler: Some(respond_to_hb),
    //         mt: MATCH_TYPE_MATCH_EXACT,
    //     };
    //     addParam(&mut listen_collection as *mut _, baby_duck);

    //     let listen_id = combinedID(enablePrecharge, vitalsID);
    //     let enablePC = CANListenParam {
    //         listen_id,
    //         handler: Some(contactorsOn),
    //         mt: MATCH_TYPE_MATCH_EXACT,
    //     };
    //     addParam(&mut listen_collection as *mut _, enablePC);

    //     let listen_id = combinedID(2, vitalsID);
    //     let disablePC = CANListenParam {
    //         listen_id,
    //         handler: Some(contactorsOff),
    //         mt: MATCH_TYPE_MATCH_EXACT,
    //     };
    //     addParam(&mut listen_collection as *mut _, disablePC);
    // }

    spawner.spawn(HBLED_task(p.PA3)).unwrap();
    spawner.spawn(contactor_control_task(p.PC6, p.PB12, p.PB14)).unwrap();
    spawner.spawn(voltage_reader_task(p.I2C1, p.PB6, p.PB7, p.DMA1_CH6, p.DMA1_CH0)).unwrap();
    spawner.spawn(imd_task(p.PA8, p.EXTI8, p.PC8)).unwrap();    
    loop {
        // rprintln!("looping");
        Timer::after(Duration::from_millis(5)).await;
        // respond_to_hb(core::ptr::null_mut());

        // unsafe { sendStatusUpdate(5, 3); }

        // Process packets (This polls the queue populated by the background task)
        // let _res = waitPackets(&mut listen_collection as *mut _);

        // --        
        // Non-blocking sleep! Allows CAN RX task to run.
        // Timer::after(Duration::from_secs(1)).await;
        Timer::after(Duration::from_millis(5)).await;
    }
}