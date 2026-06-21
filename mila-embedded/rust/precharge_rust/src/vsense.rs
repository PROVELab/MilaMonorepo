// src/vsense.rs
#![allow(dead_code)]
#![allow(non_snake_case)]

use rtt_target::rprintln;

// At the top of src/vsense.rs (add these imports)
use embassy_stm32::i2c::{I2c, Config as I2cConfig};
use embassy_stm32::time::Hertz;
use embassy_time::{Duration, Timer};
use embassy_stm32::peripherals::{I2C1, PB6, PB7, DMA1_CH6, DMA1_CH0};

use core::sync::atomic::AtomicU32;
use core::sync::atomic::Ordering;

// Move the interrupt bindings here so vsense is completely self-contained!
embassy_stm32::bind_interrupts!(struct I2cIrqs {
    I2C1_EV => embassy_stm32::i2c::EventInterruptHandler<I2C1>;
    I2C1_ER => embassy_stm32::i2c::ErrorInterruptHandler<I2C1>;
});

use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::signal::Signal;

// The Signal carries no data (()), it's just a "poke" to wake up waiters.
static NEW_VOLTAGE_SIGNAL: Signal<CriticalSectionRawMutex, ()> = Signal::new();


/// Resistor dividers (same as the C code)
const MC_R_TOP_OHMS:  i64 = 2_040_000;
const MC_R_BOT_OHMS:  i64 =   12_100;
const BAT_R_TOP_OHMS: i64 = 2_040_000;
const BAT_R_BOT_OHMS: i64 =   12_100;

/// Error type for our simple driver
#[derive(Debug)]
pub enum VsenseError<I2cErr> {
    I2cWrite(I2cErr),
    I2cRead(I2cErr),
    NotReady,      // ADC never became ready (if we add a timeout)
}

pub static MC_BUS_MV: AtomicU32 = AtomicU32::new(0);
pub static BAT_BUS_MV: AtomicU32 = AtomicU32::new(0);

//can be used by precharge, to ensure it gets a fresh reading on voltages
pub async fn get_voltages_fresh() -> (i32, i32) {
    NEW_VOLTAGE_SIGNAL.reset();

    NEW_VOLTAGE_SIGNAL.wait().await;

    (get_batteryvoltage(), get_motorvoltage())
}

pub fn get_batteryvoltage() -> i32 {
    return BAT_BUS_MV.load(Ordering::Relaxed) as i32;
}
pub fn get_motorvoltage() -> i32 {
    return MC_BUS_MV.load(Ordering::Relaxed) as i32;
}

#[embassy_executor::task]
pub async fn voltage_reader_task(
    i2c_peri: I2C1,
    scl: PB6,
    sda: PB7,
    tx_dma: DMA1_CH6,
    rx_dma: DMA1_CH0,
) {
    rprintln!("VSENSE task started: Initializing I2C...");

    // ------------------------------------------------
    // 1. Configure and Initialize I2C
    // ------------------------------------------------
    let i2c_config = I2cConfig::default();

    let mut i2c = I2c::new(
        i2c_peri,
        scl,
        sda,
        I2cIrqs,     
        tx_dma, 
        rx_dma,
        Hertz(10_000),
        i2c_config, 
    );

    let addr7: u8 = 0x69;

    loop {
        // --- Read CH1 (Motor Controller) ---
        let cfg1 = mcp3422_cfg_oneshot_ch(1);
        
        if i2c.write(addr7, &[cfg1]).await.is_ok() {
            let mut rx = [0u8; 3];
            let mut success = false;
            
            // Poll for RDY=0 with a 100ms timeout
            for _ in 0..20 {
                match i2c.read(addr7, &mut rx).await {
                    Ok(_) => {
                        let status = rx[2];
                        if (status & 0x80) == 0 { 
                            success = true;
                            break; 
                        } 
                    }
                    Err(_) => {
                        // I2C bus error, break to avoid an infinite loop
                        rprintln!("VSENSE I2C read error on CH1");
                        break; 
                    }
                }
                Timer::after(Duration::from_millis(5)).await;
            }

            if success {
                let code = i16::from_be_bytes([rx[0], rx[1]]) as i32;
                let uV: i64 = (code as i64 * 625) / 10;
                let mc_adc_mV: i32 = (uV / 1000) as i32;
                let mc_bus_mV = adc_to_bus_mV_ch(mc_adc_mV, 1);
                MC_BUS_MV.store(mc_bus_mV as u32, Ordering::Relaxed);
                
                rprintln!("VSENSE: CH1 Raw: {} mV -> Bus: {} mV", mc_adc_mV, mc_bus_mV);
            } else {
                rprintln!("VSENSE Error: CH1 RDY timeout or I2C read failure");
            }
        } else {
            rprintln!("VSENSE I2C Error: Device not responding on CH1");
        }

        // --- Read CH2 (Battery) ---
        let cfg2 = mcp3422_cfg_oneshot_ch(2);
        
        if i2c.write(addr7, &[cfg2]).await.is_ok() {
            let mut rx = [0u8; 3];
            let mut success = false;
            
            // Poll for RDY=0 with a 100ms timeout
            for _ in 0..20 {
                match i2c.read(addr7, &mut rx).await {
                    Ok(_) => {
                        let status = rx[2];
                        if (status & 0x80) == 0 { 
                            success = true;
                            break; 
                        } 
                    }
                    Err(_) => {
                        // I2C bus error, break to avoid an infinite loop
                        rprintln!("VSENSE I2C read error on CH2");
                        break; 
                    }
                }
                Timer::after(Duration::from_millis(5)).await;
            }

            if success {
                let code = i16::from_be_bytes([rx[0], rx[1]]) as i32;
                let uV: i64 = (code as i64 * 625) / 10;
                let bat_adc_mV: i32 = (uV / 1000) as i32;
                let bat_bus_mV = adc_to_bus_mV_ch(bat_adc_mV, 2);
                BAT_BUS_MV.store(bat_bus_mV as u32, Ordering::Relaxed);

                rprintln!("VSENSE: CH2 Raw: {} mV -> Bus: {} mV", bat_adc_mV, bat_bus_mV);
            } else {
                rprintln!("VSENSE Error: CH2 RDY timeout or I2C read failure");
            }
        } else {
            rprintln!("VSENSE I2C Error: Device not responding on CH2");
        }
        
        NEW_VOLTAGE_SIGNAL.signal(()); // Notify waiters that new voltage data is available (even if stale due to an error)
        
        //ADC reset check. 0V likely means a reset is needed:
        let current_bat_mv = BAT_BUS_MV.load(Ordering::Relaxed);
        
        if current_bat_mv == 0 {
            rprintln!("VSENSE Warning: Battery reads 0V! Sending General Call Reset to ADC...");
            
            // I2C General Call Address is 0x00, Reset Command is 0x06
            if i2c.write(0x00, &[0x06]).await.is_err() {
                rprintln!("VSENSE Error: Failed to send I2C reset command.");
            } else {
                rprintln!("VSENSE: ADC Reset sent successfully.");
            }
            
            // Give the MCP3422 a brief moment to reboot before the next loop
            Timer::after(Duration::from_millis(10)).await;
        }

        // Sleep for 1 second before doing it again
        Timer::after(Duration::from_secs(1)).await;
    }
}

/// Build MCP3422 config byte for one-shot conversion on ch=1 or 2,
/// 16-bit resolution, PGA=1 (exactly your C config).
fn mcp3422_cfg_oneshot_ch(ch: u8) -> u8 {
    let ch_bits = if ch == 2 { 1u8 << 5 } else { 0u8 }; // 0x20 for CH2, 0x00 for CH1
    0x80 | ch_bits | 0x10 | 0x08 | 0x00
    //  RDY=1   CH      one-shot  16-bit  PGA=1
}


/// Convert ADC pin mV to actual bus mV with the per-channel divider.
fn adc_to_bus_mV_ch(adc_mV: i32, ch: u8) -> i32 {
    if adc_mV < 0 {
        return -1;
    }
    let adc = adc_mV as i64;
    if ch == 2 {
        // Battery bus
        ((adc * (BAT_R_TOP_OHMS + BAT_R_BOT_OHMS)) / BAT_R_BOT_OHMS) as i32
    } else {
        // Motor-controller bus
        ((adc * (MC_R_TOP_OHMS + MC_R_BOT_OHMS)) / MC_R_BOT_OHMS) as i32
    }
}