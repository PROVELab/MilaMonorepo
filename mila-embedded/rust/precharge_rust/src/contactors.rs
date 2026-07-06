#![allow(non_snake_case)]

use core::sync::atomic::AtomicI32;
use core::sync::atomic::{AtomicU8, Ordering};
use embassy_futures::select::{select, Either};
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::channel::Channel;
use embassy_time::{with_timeout, Duration, Timer};
use rtt_target::rprintln;

use sensor_common::ffi::{sendStatusUpdate, CANPacket};
use sensor_common::program_constants::{
    contactorsSuccess, disableContactor, enableContactor, prechargeID,
};

use crate::vsense::get_voltages_fresh;

// ==========================================
// 1. TYPES & GLOBAL STATE
// ==========================================

#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(u8)]
pub enum PrechargeState {
    Off = 0,
    Precharging = 1,
    On = 2,
}

#[derive(Copy, Clone, Debug)]
#[repr(u8)]
pub enum ContactorCommand {
    Enable = 0,
    Disable = 1,
}

pub static CONTACTOR_STATE: AtomicU8 = AtomicU8::new(0);
pub static CONTACTOR_CHAN: Channel<CriticalSectionRawMutex, ContactorCommand, 8> = Channel::new();
static MIN_MC_VOLTAGE_MV: AtomicI32 = AtomicI32::new(80_000);
static MIN_PERCENT_CHARGED: AtomicI32 = AtomicI32::new(90);

//Entry points for command handler
#[no_mangle]
pub extern "C" fn contactorsOn(_p: *mut CANPacket) -> i16 {
    let _ = CONTACTOR_CHAN.try_send(ContactorCommand::Enable);
    0
}

#[no_mangle]
pub extern "C" fn contactorsOff(_p: *mut CANPacket) -> i16 {
    let _ = CONTACTOR_CHAN.try_send(ContactorCommand::Disable);
    0
}

#[no_mangle]
pub unsafe extern "C" fn handle_vitals_precharge_command(p: *mut CANPacket) -> i16 {
    if p.is_null() {
        return 0;
    }

    let packet = &*p;
    if packet.dataSize < 1 {
        rprintln!("ignoring empty vitals precharge command");
        return 0;
    }

    let command = packet.data[0] as u32;
    if command == enableContactor {
        let _ = CONTACTOR_CHAN.try_send(ContactorCommand::Enable);
    } else if command == disableContactor {
        let _ = CONTACTOR_CHAN.try_send(ContactorCommand::Disable);
    } else {
        rprintln!("ignoring unknown vitals precharge command {}", command);
    }

    0
}

// ==========================================
// For retrieving state from other files
// ==========================================

fn print_cmd_eval(cmd: &ContactorCommand, state: &PrechargeState) {
    rprintln!("applying command {:?} to state: {:?}", cmd, state);
}

pub fn get_precharge_state() -> i32 {
    CONTACTOR_STATE.load(Ordering::Relaxed) as i32
}

pub fn get_contactor_state() -> i32 {
    CONTACTOR_STATE.load(Ordering::Relaxed) as i32
}

pub fn get_min_mc_voltage() -> i32 {
    MIN_MC_VOLTAGE_MV.load(Ordering::Relaxed)
}

pub fn get_min_percent_charged() -> i32 {
    MIN_PERCENT_CHARGED.load(Ordering::Relaxed)
}

pub fn set_charge_condition(min_mc_voltage: i32, min_percent_charged: i32) {
    MIN_MC_VOLTAGE_MV.store(min_mc_voltage.saturating_mul(1000), Ordering::Relaxed);
    MIN_PERCENT_CHARGED.store(min_percent_charged, Ordering::Relaxed);
}

//************************ */
// MAIN FSM for controlling contactors:
//************************ */
#[embassy_executor::task]
pub async fn contactor_control_task(
    pc_pin: embassy_stm32::peripherals::PC6,
    neg_pin: embassy_stm32::peripherals::PB12,
    pos_pin: embassy_stm32::peripherals::PB14,
) {
    let mut pc = Output::new(pc_pin, Level::Low, Speed::Low);
    let mut neg = Output::new(neg_pin, Level::Low, Speed::Low);
    let mut pos = Output::new(pos_pin, Level::Low, Speed::Low);

    let mut current_state = PrechargeState::Off;
    loop {
        // The single match block handles Output -> Wait -> Print -> Parse
        match current_state {
            // ==========================================
            // STATE: OFF
            // ==========================================
            PrechargeState::Off => {
                //set output
                pc.set_low();
                neg.set_low();
                pos.set_low();
                CONTACTOR_STATE.store(0, Ordering::Relaxed);

                //wait for command
                let cmd = CONTACTOR_CHAN.receive().await;
                print_cmd_eval(&cmd, &current_state);

                //set next state
                match cmd {
                    ContactorCommand::Enable => current_state = PrechargeState::Precharging,
                    _ => rprintln!("ignoring command"),
                }
            }

            // ==========================================
            // STATE: PRECHARGING
            // ==========================================
            PrechargeState::Precharging => {
                //check if safe

                //set output
                pc.set_high();
                neg.set_high();
                pos.set_low();
                CONTACTOR_STATE.store(1, Ordering::Relaxed);

                //start a timer for precharge complete
                let mut timeout = Timer::after(Duration::from_secs(5));

                //the same timer persists until exiting precharge
                while current_state == PrechargeState::Precharging {
                    //wait for command (or timer), then set next state.
                    match select(&mut timeout, CONTACTOR_CHAN.receive()).await {
                        Either::First(_) => {
                            //5 seconds have passed, try entering the on state
                            current_state = PrechargeState::On;
                        }
                        Either::Second(cmd) => {
                            print_cmd_eval(&cmd, &current_state);
                            match cmd {
                                ContactorCommand::Disable => {
                                    current_state = PrechargeState::Off;
                                }
                                _ => rprintln!("ignoring command"),
                            }
                        }
                    }
                }
            }

            // ==========================================
            // STATE: ON
            // ==========================================
            PrechargeState::On => {
                //check if safe to go on

                match with_timeout(Duration::from_millis(2500), get_voltages_fresh()).await {
                    Ok((v_bat, v_mc)) => {
                        let min_percent_charged = MIN_PERCENT_CHARGED.load(Ordering::Relaxed);
                        let min_mc_voltage_mv = MIN_MC_VOLTAGE_MV.load(Ordering::Relaxed);
                        if (v_mc < (v_bat * min_percent_charged) / 100) || v_mc < min_mc_voltage_mv
                        {
                            current_state = PrechargeState::Precharging;
                            rprintln!("not precharged yet. returning to precharge (vbat: {} mV, vmotor: {} mV)", v_bat, v_mc);
                            continue;
                        }
                        unsafe {
                            sendStatusUpdate(contactorsSuccess as u8, prechargeID);
                        }
                    }
                    Err(_) => {
                        current_state = PrechargeState::Off;
                        rprintln!("Precharge voltage read timeout (very bad)");
                        continue;
                    }
                }

                //set output
                pos.set_high();
                //give some time for pos contactor to open
                Timer::after(Duration::from_millis(10)).await;
                pc.set_low();
                neg.set_high();
                CONTACTOR_STATE.store(2, Ordering::Relaxed);

                //wait for command
                let cmd = CONTACTOR_CHAN.receive().await;
                print_cmd_eval(&cmd, &current_state);

                //set next state
                match cmd {
                    ContactorCommand::Disable => current_state = PrechargeState::Off,
                    _ => rprintln!("ignoring command"),
                }
            }
        }
    }
}
