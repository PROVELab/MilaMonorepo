// src/hw.rs

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

use core::cell::RefCell;
use cortex_m::interrupt::Mutex;
use cortex_m::interrupt as cm_interrupt;

// --- Embassy Sync Imports for Channel ---
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::channel::Channel;

// --- Embassy CAN Imports ---
use embassy_stm32::can::{
    Can, CanTx, CanRx, 
    Rx0InterruptHandler, Rx1InterruptHandler, TxInterruptHandler, SceInterruptHandler,
};
use embassy_stm32::can::bxcan::{
    Frame, StandardId, ExtendedId, Id, Data, Fifo, // Added Fifo here
};
use embassy_stm32::can::bxcan::filter::Mask32;     // Added Mask32 here
use embassy_stm32::peripherals::CAN1;
use embassy_executor::Spawner;
use static_cell::StaticCell;
use embassy_time::Duration;
use rtt_target::rprintln;

use embassy_stm32::bind_interrupts;

// F405 CAN1 Interrupt Mapping
bind_interrupts!(struct Irqs {
    CAN1_RX0 => Rx0InterruptHandler<CAN1>;
    CAN1_RX1 => Rx1InterruptHandler<CAN1>;
    CAN1_TX => TxInterruptHandler<CAN1>;
    CAN1_SCE => SceInterruptHandler<CAN1>;
});

use crate::ffi;
use crate::ffi::{
    CANPacket,
    MAX_SIZE_PACKET_DATA,
    exact,
    matchID,
    matchFunction,
    sendStatusUpdate,
    pecanInit,
    PCANListenParamsCollection,
};

// ----------------- Global TX Handle -----------------
pub static PECAN_GLOBAL_TX: Mutex<RefCell<Option<CanTx<'static, 'static, CAN1>>>> = Mutex::new(RefCell::new(None));

pub fn init_can_tx(tx: CanTx<'static, 'static, CAN1>) {
    cm_interrupt::free(|cs| {
        PECAN_GLOBAL_TX.borrow(cs).replace(Some(tx));
    });
}

// ----------------- RX Channel (The Embassy Way) -----------------
// A thread-safe async queue of 16 CANPackets. Replaces the static mut ring buffer.
static RX_CHANNEL: Channel<CriticalSectionRawMutex, CANPacket, 16> = Channel::new();

// ----------------- ASYNC RX TASK -----------------
#[embassy_executor::task]
pub async fn can_rx_task(mut rx: CanRx<'static, 'static, CAN1>) {
    loop {
        match rx.read().await {
            Ok(envelope) => {
                let frame = envelope.frame;
                
                // Initialize a clean packet
                let mut pkt = CANPacket {
                    data: [0; MAX_SIZE_PACKET_DATA as usize],
                    id: 0,
                    dataSize: 0,
                    rtr: false,
                    extendedID: false,
                };
                
                // 1. Map ID
                match frame.id() {
                    Id::Standard(sid) => {
                        pkt.id = sid.as_raw() as i32;
                        pkt.extendedID = false;
                    }
                    Id::Extended(eid) => {
                        pkt.id = eid.as_raw() as i32;
                        pkt.extendedID = true;
                    }
                }

                // 2. Map RTR Flag
                pkt.rtr = frame.is_remote_frame();
                
                // 3. Map Data Payload
                if let Some(data) = frame.data() {
                    let len = core::cmp::min(data.len(), 8); 
                    pkt.dataSize = len as u8;
                    pkt.data[..len].copy_from_slice(&data.as_ref()[..len]);
                } else {
                    pkt.dataSize = 0;
                }

                // 4. Send to the channel (non-blocking)
                if let Err(_) = RX_CHANNEL.try_send(pkt) {
                    rprintln!("RX QUEUE OVERRUN - Dropping packet!");
                }
            }
            Err(_) => {
                rprintln!("CAN RX Error (Bus Off / Stuffing Error)");
            }
        }
    }
}

// ----------------- CAN INITIALIZATION -----------------
pub static mut NODE_ID: i32 = 0;
static MATCHER: [unsafe extern "C" fn(u32, u32) -> bool; 3] = [exact, matchID, matchFunction];
static CAN_INSTANCE: StaticCell<Can<'static, CAN1>> = StaticCell::new();

#[no_mangle]
pub async fn pecan_CanInit(
    config: pecanInit, 
    spawner: &Spawner, 
    can1: embassy_stm32::peripherals::CAN1, 
    rx_pin: embassy_stm32::peripherals::PA11, 
    tx_pin: embassy_stm32::peripherals::PA12
) {
    let mut can = Can::new(can1, rx_pin, tx_pin, Irqs);

    // 1. Open the Filter Wall
    // Allow ALL incoming packets to pass through to FIFO 0 so the async task can read them.
    can.as_mut().modify_filters().enable_bank(
        0,              // Filter bank index
        Fifo::Fifo0,    // Send matching packets to FIFO 0
        Mask32::accept_all()
    );

    // 2. Set to Normal Mode (False = Normal, True = Loopback)
    // can.as_mut().modify_config().set_loopback(false).leave_disabled();
    can.as_mut().modify_config().set_loopback(false);
    
    // 3. Set Bitrate (relies on the 42MHz APB1 clock from main.rs)
    can.set_bitrate(500_000);
    
    // 4. Enable the hardware state machine
    can.enable().await; 

    // Move to static memory for the tx/rx split
    let can_static = CAN_INSTANCE.init(can);
    let (tx, rx) = can_static.split();

    init_can_tx(tx);
    spawner.spawn(can_rx_task(rx)).unwrap();

    unsafe {
        NODE_ID = config.nodeId; 
        sendStatusUpdate(0, NODE_ID as u32);
    }
    rprintln!("CAN System Online: Filters Open, 500kbps, Normal Mode");
}

// ----------------- SYNCHRONOUS SEND (C-FFI) -----------------
#[no_mangle]
pub extern "C" fn sendPacket(p: *mut CANPacket) {
    if p.is_null() { return; }
    let pkt = unsafe { &mut *p };

    let id_raw = pkt.id as u32;
    let id = if pkt.extendedID {
        Id::Extended(ExtendedId::new(id_raw & 0x1FFF_FFFF).unwrap())
    } else {
        Id::Standard(StandardId::new((id_raw & 0x7FF) as u16).unwrap())
    };

    let frame = if pkt.rtr {
        Frame::new_remote(id, pkt.dataSize)
    } else {
        let len = core::cmp::min(pkt.dataSize as usize, 8);
        let data = Data::new(&pkt.data[..len]).unwrap(); 
        Frame::new_data(id, data)
    };

    let mut retry_count: u32 = 0;
    const MAX_RETRIES: u32 = 1000; 

    loop {
        let mut success = false;
        let mut initialized = false;

        cm_interrupt::free(|cs| {
            let mut tx_ref = PECAN_GLOBAL_TX.borrow(cs).borrow_mut();
            if let Some(tx) = tx_ref.as_mut() {
                initialized = true;
                if let Ok(_) = tx.try_write(&frame) {
                    success = true;
                }
            }
        });

        if !initialized {
            rprintln!("TX ERROR: Driver not initialized!");
            return;
        }

        if success {
            rprintln!("TX Success: ID {:#x}", id_raw);
            break; 
        }

        retry_count += 1;
        if retry_count >= MAX_RETRIES {
            rprintln!("TX TIMEOUT: Bus full/No ACK");
            break;
        }

        embassy_time::block_for(Duration::from_micros(100)); 
    }
}

// ----------------- WAIT PACKETS (C-FFI) -----------------
#[no_mangle]
pub fn waitPackets(plpc: *mut PCANListenParamsCollection) -> i16 {
    if plpc.is_null() { return ffi::PCAN_ERR_NOT_RECEIVED as i16; }
    
    // Non-blocking read from the Embassy Channel
    let mut recv = match RX_CHANNEL.try_receive() {
        Ok(p) => p,
        Err(_) => return ffi::PCAN_ERR_NOT_RECEIVED as i16,
    };

    rprintln!("Received Packet! ID: {:#x}", recv.id);

    // Data normalization logic for C code
    if recv.extendedID {
        recv.id &= 0x1FFF_FFFF;
    } else {
        recv.id &= 0x7FF;
    }

    if recv.rtr {
        recv.dataSize = 0;
    } else {
        let sz = recv.dataSize as usize;
        let clamped = if sz > (MAX_SIZE_PACKET_DATA as usize) {
            MAX_SIZE_PACKET_DATA as usize
        } else {
            sz
        };
        recv.dataSize = clamped as u8;
    }

    let used = recv.dataSize as usize;
    if used < (MAX_SIZE_PACKET_DATA as usize) {
        for b in &mut recv.data[used..(MAX_SIZE_PACKET_DATA as usize)] {
            *b = 0;
        }
    }

    // Matcher and Handler logic
    unsafe {
        let coll = &mut *plpc;
        for i in 0..coll.size {
            let clp = &coll.arr[i as usize];
            let mt_idx = clp.mt as i32;
            if mt_idx >= 0 && (mt_idx as usize) < MATCHER.len() {
                let matcher = MATCHER[mt_idx as usize];
                if matcher(recv.id as u32, clp.listen_id) {
                    if let Some(handler) = clp.handler {
                        rprintln!("Routing to specific handler");
                        return handler(&mut recv as *mut CANPacket);
                    }
                }
            }
        }
        
        if let Some(default_handler) = coll.defaultHandler {
            rprintln!("Routing to default handler");
            return default_handler(&mut recv as *mut CANPacket);
        }
    }

    ffi::PCAN_ERR_NOT_RECEIVED as i16
}