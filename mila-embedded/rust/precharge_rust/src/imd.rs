#![allow(non_snake_case)]

use embassy_stm32::gpio::{Input, Pull};
use embassy_stm32::exti::ExtiInput;
use embassy_time::{with_timeout, Duration, Instant};
use rtt_target::rprintln;
use core::sync::atomic::{AtomicI32, AtomicU8, Ordering};


#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ImdFreqState {
    Normal,             // 10 Hz
    Undervoltage,       // 20 Hz
    Startup,            // 30 Hz
    DeviceFault,        // 40 Hz
    EarthFault,         // 50 Hz
    ShortOrPoweredOff,  // 0 Hz
    UnknownFreq,        // Outside expected bands
}

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum ImdDutyState {
    Safe,           // 5 - 10 %
    Unsafe,         // 90 - 95 %
    InvalidPwm,     // Duty cycle doesn't matter (Fault/Startup states)
    UnknownDuty,    // Outside expected bands
}

static IMD_FREQUENCY_HZ: AtomicI32 = AtomicI32::new(0);
static IMD_DUTY_CYCLE_PERCENT: AtomicI32 = AtomicI32::new(0);
static IMD_HARDWARE_FAULT_INDICATOR: AtomicU8 = AtomicU8::new(0);

// Main task, measure, evaluate, and report IMD status.
#[embassy_executor::task]
pub async fn imd_task(
    pwm_pin: embassy_stm32::peripherals::PA8,
    exti_ch: embassy_stm32::peripherals::EXTI8, 
    fault_pin: embassy_stm32::peripherals::PC8,
) {
    let mut pwm_in = ExtiInput::new(Input::new(pwm_pin, Pull::None), exti_ch);
    let fault_in = Input::new(fault_pin, Pull::Up); 
    let mut last_print_time = Instant::now();

    rprintln!("IMD Task Started. Monitoring IR155...");

    loop {
        let (freq_hz, duty_cycle) = measure_signal(&mut pwm_in).await;
        let (freq_state, duty_state) = evaluate_signal(freq_hz, duty_cycle);
        let is_imd_ok = fault_in.is_high();

        report_telemetry(
            freq_hz, duty_cycle, 
            freq_state, duty_state, 
            is_imd_ok, &mut last_print_time
        );
    }
}


/// Asynchronously measure the PWM signal
async fn measure_signal(pwm_in: &mut ExtiInput<'_, impl embassy_stm32::gpio::Pin>) -> (f32, f32) {
    let measure_fut = async {
        pwm_in.wait_for_rising_edge().await;
        let t_rise1 = Instant::now();
        
        pwm_in.wait_for_falling_edge().await;
        let t_fall = Instant::now();
        
        pwm_in.wait_for_rising_edge().await;
        let t_rise2 = Instant::now();
        
        (t_rise1, t_fall, t_rise2)
    };

    // 500ms timeout catches anything below 2Hz
    match with_timeout(Duration::from_millis(500), measure_fut).await {
        Ok((t_rise1, t_fall, t_rise2)) => {
            let period_us = t_rise2.duration_since(t_rise1).as_micros() as f32;
            let high_us = t_fall.duration_since(t_rise1).as_micros() as f32;
            
            let freq_hz = 1_000_000.0 / period_us;
            let duty_cycle = (high_us / period_us) * 100.0;
            
            (freq_hz, duty_cycle)
        }
        Err(_) => {
            // Timeout occurred, signal is flat
            let freq_hz = 0.0;
            let duty_cycle = if pwm_in.is_high() { 100.0 } else { 0.0 };
            (freq_hz, duty_cycle)
        }
    }
}

/// Apply the IR155 interpretation chart
fn evaluate_signal(freq_hz: f32, duty_cycle: f32) -> (ImdFreqState, ImdDutyState) {
    // 1. Evaluate Frequency (+/- 2Hz tolerance)
    let freq_state = match freq_hz {
        f if f >= 8.0  && f <= 12.0 => ImdFreqState::Normal,
        f if f >= 18.0 && f <= 22.0 => ImdFreqState::Undervoltage,
        f if f >= 28.0 && f <= 32.0 => ImdFreqState::Startup,
        f if f >= 38.0 && f <= 42.0 => ImdFreqState::DeviceFault,
        f if f >= 48.0 && f <= 52.0 => ImdFreqState::EarthFault,
        f if f < 2.0                => ImdFreqState::ShortOrPoweredOff,
        _                           => ImdFreqState::UnknownFreq,
    };

    // 2. Evaluate Duty Cycle with tolerance
    let duty_state = if freq_state == ImdFreqState::Normal || freq_state == ImdFreqState::Undervoltage {
        match duty_cycle {
            d if d >= 3.0  && d <= 15.0 => ImdDutyState::Safe,
            d if d >= 85.0 && d <= 97.0 => ImdDutyState::Unsafe,
            _                           => ImdDutyState::UnknownDuty,
        }
    } else {
        ImdDutyState::InvalidPwm 
    };

    (freq_state, duty_state)
}

//store the current state in atomic variables, that can be retrieved from other functions 
//like the sensor data collection upon request

fn report_telemetry(freq_hz: f32, duty_cycle: f32, freq_state: ImdFreqState,
    duty_state: ImdDutyState, is_imd_ok: bool, last_print: &mut Instant
) {
    IMD_FREQUENCY_HZ.store(freq_hz as i32, Ordering::Relaxed);
    IMD_DUTY_CYCLE_PERCENT.store(duty_cycle as i32, Ordering::Relaxed);
    IMD_HARDWARE_FAULT_INDICATOR.store(if is_imd_ok { 0 } else { 1 }, Ordering::Relaxed);

    if last_print.elapsed() >= Duration::from_secs(1) {
        let ok_str = if is_imd_ok { "HIGH (OK)" } else { "LOW (FAULT)" };
        
        rprintln!(
            "IMD | {:.1}Hz -> {:?} | {:.1}% -> {:?} | IMD_OK: {}",
            freq_hz, freq_state, duty_cycle, duty_state, ok_str
        );
        
        *last_print = Instant::now();
    }
}

pub fn get_imd_frequency_hz() -> i32 {
    IMD_FREQUENCY_HZ.load(Ordering::Relaxed)
}

pub fn get_imd_duty_cycle_percent() -> i32 {
    IMD_DUTY_CYCLE_PERCENT.load(Ordering::Relaxed)
}

pub fn get_imd_hardware_fault_indicator() -> i32 {
    IMD_HARDWARE_FAULT_INDICATOR.load(Ordering::Relaxed) as i32
}
