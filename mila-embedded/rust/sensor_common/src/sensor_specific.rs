use embassy_executor::Spawner;
use embassy_time::Duration;
use rtt_target::rprintln;

use crate::ffi::{
    PCANListenParamsCollection, myId, myframes, numFrames, sendFrame, vitalsInit, registerCommandHandler
};

const MAX_FRAMES: usize =  numFrames as usize;

#[embassy_executor::task(pool_size = MAX_FRAMES)]
async fn send_frame_task(frame_num: usize) {
    let period = unsafe { myframes[frame_num].period as u64 };
    let mut ticker = embassy_time::Ticker::every(Duration::from_millis(period));

    loop {
        unsafe { sendFrame(frame_num as i8) };
        ticker.next().await;
    }
}

pub fn sensor_init(spawner: &Spawner, plpc: &mut PCANListenParamsCollection) {
    unsafe {
        vitalsInit(plpc, myId as u16);
    }

    let num_frames_total = numFrames as usize;
    for i in 0..num_frames_total {
        spawner.spawn(send_frame_task(i)).unwrap();
    }
    rprintln!("Initializing Rust-specific sensor tasks for node {}...", myId);
    unsafe{
        registerCommandHandler(plpc);
    }
}
