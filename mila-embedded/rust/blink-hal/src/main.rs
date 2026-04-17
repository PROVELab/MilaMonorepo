#![no_std]
#![no_main]
#![allow(non_snake_case)]

use panic_halt as _;
use rtt_target::{rtt_init_print, rprintln};

use embassy_executor::Spawner;
use embassy_time::{with_timeout, Duration, Timer};
use embassy_stm32::time::Hertz;
use embassy_stm32::gpio::{Level, Output, Speed, Pull};
//for pullup on i2c:
use embassy_stm32::gpio::low_level::Pin; // Unlocks set_as_af_pull
use embassy_stm32::i2c::SclPin; // Unlocks af_num for SCL
use embassy_stm32::i2c::SdaPin; // Unlocks af_num for SDA
//
use embassy_stm32::i2c::I2c; 
use embassy_stm32::can::Can; 
use embassy_stm32::rcc::{Hse, HseMode, Sysclk, Pll, PllSource, PllPreDiv, PllMul, PllPDiv};

use embassy_stm32::peripherals::CAN1;
use embassy_stm32::{bind_interrupts, Config};

// F405 CAN1 requires 4 handlers; I2C1 requires 2.
bind_interrupts!(struct Irqs {
    CAN1_RX0 => embassy_stm32::can::Rx0InterruptHandler<CAN1>;
    CAN1_RX1 => embassy_stm32::can::Rx1InterruptHandler<CAN1>;
    CAN1_TX => embassy_stm32::can::TxInterruptHandler<CAN1>;
    CAN1_SCE => embassy_stm32::can::SceInterruptHandler<CAN1>;
    I2C1_EV => embassy_stm32::i2c::EventInterruptHandler<embassy_stm32::peripherals::I2C1>;
    I2C1_ER => embassy_stm32::i2c::ErrorInterruptHandler<embassy_stm32::peripherals::I2C1>;
});

mod ffi {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod hw;
mod vsense;

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
    functionCodes_HBPing,
    functionCodes_HBPong,
    specialIDs_vitalsID,
    specialIDs_prechargeID,
    MATCH_TYPE_MATCH_EXACT,
};
use crate::hw::{can_rx_task, sendPacket, waitPackets, PECAN_GLOBAL_TX};

use static_cell::StaticCell;

// Create an empty, safe static cell
static CAN_STATE: StaticCell<Can<'static, CAN1>> = StaticCell::new();

// Define interrupt handler types for brevity
use embassy_stm32::can::Rx0InterruptHandler;
use embassy_stm32::can::TxInterruptHandler;

#[no_mangle]
pub extern "C" fn respond_to_hb(_p: *mut CANPacket) -> i16 {
    rprintln!("responding to HBPing");

    let mut pkt = CANPacket {
        data: [0; MAX_SIZE_PACKET_DATA as usize],
        id: 0,
        dataSize: 0,
        rtr: true,
        extendedID: false,
    };

    unsafe {
        pkt.id = combinedID(functionCodes_HBPong, specialIDs_prechargeID) as i32;
    }

    sendPacket(&mut pkt as *mut CANPacket); 
    return 0;
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {

    // 1. Initialize RTT
    rtt_init_print!();
    
    // 2. Configure Clocks (80 MHz HSI match)
    let mut config = Config::default();
    //temp?

    config.enable_debug_during_sleep = true;    //so ST-Link doesnt get destroyed when the stm sleeps.

    let p = embassy_stm32::init(config);
    
    Timer::after_millis(1000).await;
    rprintln!("*** ENTERED MAIN (Embassy) ***");

    // 3. GPIO Setup
    let mut led = Output::new(p.PA3, Level::Low, Speed::Low);

    // 4. I2C Setup (Async)
    // PB6=SCL, PB7=SDA (Using I2C1)
    // let mut i2c1 = I2c::new(
    //     p.I2C1,
    //     p.PB6,
    //     p.PB7,
    //     Irqs,
    //     p.DMA1_CH6, // DMA streams for Async I2C
    //     p.DMA1_CH0,
    //     Hertz(100_000),
    //     Default::default(),
    // );

    let mut i2c_config = embassy_stm32::i2c::Config::default();
    i2c_config.sda_pullup = true; // Turn on the "Life Support" pull-ups
    i2c_config.scl_pullup = true;

    // 2. Initialize I2C with that config
    // Note: We don't need any manual pin.set_as_af_pull calls anymore!
    let mut i2c1 = I2c::new(
        p.I2C1,
        p.PB6,
        p.PB7,
        Irqs,
        p.DMA1_CH6, 
        p.DMA1_CH0,
        Hertz(100_000),
        i2c_config, // Use our custom config here
    );

    rprintln!("--- I2C SCAN START ---");
    for addr in 1u8..128 {
        // Send a single dummy 0 byte to see if anyone ACKs
        let result = with_timeout(
            Duration::from_millis(50), 
            i2c1.write(addr, &[0]) 
        ).await;

        match result {
            Ok(Ok(_)) => rprintln!("Found device at: {:#x}", addr),
            Ok(Err(_)) => { /* No device here, silent continue */ } 
            Err(_) => rprintln!("Bus locked up/Timeout at {:#x}", addr), 
        }
        // Small delay to let the bus settle between pings
        Timer::after_millis(5).await;
    }
    rprintln!("--- I2C SCAN END ---");

    // 5. CAN Setup
    // PA11=RX, PA12=TX
    let mut can = Can::new(p.CAN1, p.PA11, p.PA12, Irqs);
    can.set_bitrate(500_000);
    can.enable().await;

    let can_static = CAN_STATE.init(can); 

    let (tx, rx) = can_static.split();

    crate::hw::init_can_tx(tx);

    // Spawn the background RX task
    spawner.spawn(can_rx_task(rx)).unwrap();


    // 6. Logic Initialization
    let cfg = pecanInit {
        nodeId: 1,
        pin1: -1,
        pin2: -1,
    };
    
    // We call this directly. Note: In original code this set a global NODE_ID.
    crate::hw::pecan_CanInit(cfg);

    let mut listen_collection: PCANListenParamsCollection = unsafe { core::mem::zeroed() };

    unsafe {
        let listen_id = combinedID(functionCodes_HBPing, specialIDs_vitalsID);
        let baby_duck = CANListenParam {
            listen_id,
            handler: Some(respond_to_hb),
            mt: MATCH_TYPE_MATCH_EXACT,
        };
        addParam(&mut listen_collection as *mut _, baby_duck);
    }

    let mcp3422_addr7: u8 = 0x69;

    // 7. Main Loop
    loop {
        rprintln!("looping");
        led.set_high();

        unsafe { sendStatusUpdate(5, 3); }

        // Process packets (This polls the queue populated by the background task)
        let _res = waitPackets(&mut listen_collection as *mut _);

        // VSENSE (Async or Blocking is fine here, assuming vsense uses the i2c ref)
        // Since vsense::read... takes &mut I2c, we pass our async i2c.
        // NOTE: You may need to update vsense to use `write_read_async` or `blocking_write_read`.
        // For simplicity, we assume vsense is updated or we use blocking calls on the async driver.
        match i2c1.blocking_write_read(mcp3422_addr7, &[/*config*/], &mut [0u8; 2]) {
             Ok(_) => rprintln!("Read OK"), // simplified for example
             Err(_) => rprintln!("VSENSE Read Error"),
        }

        led.set_low();
        
        // Non-blocking sleep! Allows CAN RX task to run.
        Timer::after(Duration::from_secs(1)).await;
    }
}