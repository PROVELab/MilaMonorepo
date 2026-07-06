use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_time::{Duration, Timer};
#[embassy_executor::task]
pub async fn HBLED_task(HB_led_pin: embassy_stm32::peripherals::PA3) {
    let mut led = Output::new(HB_led_pin, Level::Low, Speed::Low);

    loop {
        led.set_high();
        Timer::after(Duration::from_millis(500)).await;
        led.set_low();
        Timer::after(Duration::from_millis(500)).await;
    }
}
