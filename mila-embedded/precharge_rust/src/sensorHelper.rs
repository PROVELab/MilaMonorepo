#![no_std]

use crate::bindings::*; // Assuming bindgen outputs here
use embassy_time::{Duration, Timer};
use embassy_executor::Spawner;

// Embassy requires a known task pool size at compile time. 
// Set this to a generous upper bound for the max number of frames any sensor might send.
const MAX_FRAMES: usize = 10;

/// The async task that replaces the FreeRTOS xTimer callback.
/// It wakes up at the given frequency, triggers the data collection, and sends the frame.
#[embassy_executor::task(pool_size = MAX_FRAMES)]
pub async fn send_frame_task(frame_num: usize, data_collectors: &'static [fn(&mut bool) -> i32]) {
    // Fetch frequency from C-side struct
    let frequency = unsafe { myframes[frame_num].frequency as u64 };

    loop {
        // Wait for frequency period (Equivalent to xTimer reload)
        Timer::after(Duration::from_millis(frequency)).await;

        send_frame(frame_num, data_collectors);
    }
}

/// Core function to collect, pack, and queue the CAN frame
pub fn send_frame(frame_num: usize, data_collectors: &[fn(&mut bool) -> i32]) {
    unsafe {
        if frame_num >= numFrames as usize {
            // Out of bounds logging (equivalent to ESP_LOGE)
            // defmt::error!("attempted to send out of bounds frame. not sending!");
            return;
        }

        let frame = &myframes[frame_num];
        let num_data = frame.numData as usize;
        let start_idx = frame.startingDataIndex as usize;
        
        let mut curr_bit: i8 = 0;
        let mut temp_data: [u8; 8] = [0; 8];

        for i in 0..num_data {
            let mut send_frame_flag = true;
            
            // Execute the specific data collector mapped to this index
            let data = data_collectors[start_idx + i](&mut send_frame_flag);

            if !send_frame_flag {
                // Collector requested to cancel the frame send
                // defmt::info!("data collector requested to cancel frame send");
                return;
            }

            // Pointer arithmetic to get &myframes[frameNum].dataInfo[i]
            let info = frame.dataInfo.add(i);
            pecan_pack(temp_data.as_mut_ptr(), &mut curr_bit, data, info);
        }

        // Initialize empty packet
        let mut data_packet: core::mem::MaybeUninit<CANPacket> = core::mem::MaybeUninit::zeroed();
        let mut data_packet = data_packet.assume_init();

        data_packet.extendedID = 1;
        // combinedIDExtended macro/func from pecan
        data_packet.id = combinedIDExtended(transmitData as u32, myId as u32, frame_num as u32);

        // Populate length and data buffer
        let length = ((7 + curr_bit) / 8) as u8;
        writeData(&mut data_packet, temp_data.as_mut_ptr() as *mut i8, length);

        sendPacket(&mut data_packet);
    }
}

/// Vitals compliance and scheduling entry point. 
/// Replaces sensorInit().
pub fn sensor_init(
    spawner: &Spawner,
    plpc: *mut PCANListenParamsCollection,
    data_collectors: &'static [fn(&mut bool) -> i32]
) {
    unsafe {
        vitalsInit(plpc, myId as i32); // Setup heartbeats
    }

    let num_frames_total = unsafe { numFrames as usize };
    for i in 0..num_frames_total {
        // Spawn the background worker for each frame index.
        // Will panic if num_frames_total exceeds MAX_FRAMES pool size.
        spawner.spawn(send_frame_task(i, data_collectors)).unwrap();
    }
}