#![allow(non_snake_case)]

use embassy_time::{Duration, Timer, with_timeout};
use embassy_stm32::gpio::{Output, Level, Speed};
use core::sync::atomic::{AtomicU8, Ordering};
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::channel::Channel;
use embassy_futures::select::{select, Either};
use rtt_target::rprintln;

use crate::ffi::CANPacket;

use crate::vsense::get_voltages_fresh;

// ==========================================
// 1. TYPES & GLOBAL STATE
// ==========================================

#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(u8)]
pub enum PrechargeState { Off = 0, Precharging = 1, On = 2 }

#[derive(Copy, Clone, Debug)]
#[repr(u8)]
pub enum ContactorCommand {
    Enable = 0,
    Disable = 1,
    LatchOn = 2,
    LatchOff = 3,
    RemoveLatch = 4,
}

pub static CONTACTOR_STATE: AtomicU8 = AtomicU8::new(0);
pub static CONTACTOR_CHAN: Channel<CriticalSectionRawMutex, ContactorCommand, 8> = Channel::new();

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
pub extern "C" fn contactorsLatchOn(_p: *mut CANPacket) -> i16 {
    let _ = CONTACTOR_CHAN.try_send(ContactorCommand::LatchOn);
    0
}

#[no_mangle]
pub extern "C" fn contactorsLatchOff(_p: *mut CANPacket) -> i16 {
    let _ = CONTACTOR_CHAN.try_send(ContactorCommand::LatchOff);
    0
}

#[no_mangle]
pub extern "C" fn contactorsRemoveLatch(_p: *mut CANPacket) -> i16 {
    let _ = CONTACTOR_CHAN.try_send(ContactorCommand::RemoveLatch);
    0
}

// ==========================================
// 2. HELPER LOG FUNCTION
// ==========================================

fn print_cmd_eval(cmd: &ContactorCommand, state: &PrechargeState, is_latched: bool) {
    let latch_str = if is_latched { "On" } else { "Off" };
    rprintln!("applying command {:?} to state: [{:?}, latch {}]", cmd, state, latch_str);
}

// ==========================================
// 4. THE ENGINE (The Monolithic FSM)
// ==========================================

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
    // let mut is_latched: bool = true; // Start latched for safety. Must receive explicit command to unlatch (from Lora telemetry)
    let mut is_latched: bool = false; // TEMPORARY for test
    loop {
        // The single match block handles Output -> Wait -> Print -> Parse
        match current_state {
            
            // ==========================================
            // STATE: OFF
            // ==========================================
            PrechargeState::Off => {
                //set output
                pc.set_low(); neg.set_low(); pos.set_low();
                CONTACTOR_STATE.store(0, Ordering::Relaxed);

                //wait for command
                let cmd = CONTACTOR_CHAN.receive().await;
                print_cmd_eval(&cmd, &current_state, is_latched);

                //set next state
                match cmd {
                    ContactorCommand::Enable if !is_latched => current_state = PrechargeState::Precharging,
                    ContactorCommand::LatchOn => { is_latched = true; current_state = PrechargeState::Precharging; }
                    ContactorCommand::LatchOff => is_latched = true,
                    ContactorCommand::RemoveLatch => is_latched = false,
                    _ => rprintln!("ignoring command"),
                }
            }

            // ==========================================
            // STATE: PRECHARGING
            // ==========================================
            PrechargeState::Precharging => {
                //check if safe

                //set output
                pc.set_high(); neg.set_high(); pos.set_low();
                CONTACTOR_STATE.store(1, Ordering::Relaxed);

                //start a timer for precharge complete
                let mut timeout = Timer::after(Duration::from_secs(5));
                
                //the same timer persists until exiting precharge
                while(current_state == PrechargeState::Precharging) {
                    //wait for command (or timer), then set next state.
                    match select(&mut timeout, CONTACTOR_CHAN.receive()).await {
                        Either::First(_) => {
                            current_state = PrechargeState::On;
                        }
                        Either::Second(cmd) => {
                            print_cmd_eval(&cmd, &current_state, is_latched);
                            match cmd {
                                ContactorCommand::Disable if !is_latched => {
                                    current_state = PrechargeState::Off;
                                }
                                ContactorCommand::LatchOn => is_latched = true,
                                ContactorCommand::LatchOff => { is_latched = true; current_state = PrechargeState::Off; }
                                ContactorCommand::RemoveLatch => is_latched = false,
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

                match with_timeout(Duration::from_millis(1500), get_voltages_fresh()).await {
                    Ok((v_bat, v_mc)) => {
                        if v_mc < (v_bat * 8) / 10 {
                            current_state = PrechargeState::Precharging;
                            rprintln!("not precharged yet. returning to precharge (vbat: {} mV, vmotor: {} mV)", v_bat, v_mc);
                            continue;
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
                pc.set_low(); neg.set_high();
                CONTACTOR_STATE.store(2, Ordering::Relaxed);

                //wait for command
                let cmd = CONTACTOR_CHAN.receive().await;
                print_cmd_eval(&cmd, &current_state, is_latched);

                //set next state
                match cmd {
                    ContactorCommand::Disable if !is_latched => current_state = PrechargeState::Off,
                    ContactorCommand::LatchOff => { is_latched = true; current_state = PrechargeState::Off; }
                    ContactorCommand::LatchOn => is_latched = true,
                    ContactorCommand::RemoveLatch => is_latched = false,
                    _ => rprintln!("ignoring command"),
                }
            }
        }
    }
}