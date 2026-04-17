// src/hw.rs

#![allow(non_snake_case)]
#![allow(non_camel_case_types)]

use core::cell::RefCell;
use core::cmp;

use cortex_m::interrupt::Mutex;
use cortex_m::interrupt as cm_interrupt;

use embassy_stm32::can::bxcan::{Frame, StandardId, ExtendedId, Id, Data};
use embassy_stm32::can::{CanRx, CanTx};
use embassy_stm32::peripherals::CAN1;


use crate::ffi;
use crate::ffi::{
    CANPacket,
    CANListenParam,
    PCANListenParamsCollection,
    MAX_SIZE_PACKET_DATA,
    exact,
    matchID,
    matchFunction,
    sendStatusUpdate,
    pecanInit,
};

// ----------------- Global TX Handle -----------------
// We need to store the TX part of the driver to use it in `sendPacket` (which is extern C)
// We use a Mutex<RefCell<Option<...>>> pattern to allow safe global access.
pub static PECAN_GLOBAL_TX: Mutex<RefCell<Option<CanTx<'static, 'static, CAN1>>>> = Mutex::new(RefCell::new(None));

pub fn init_can_tx(tx: CanTx<'static, 'static, CAN1>) {
    cm_interrupt::free(|cs| {
        PECAN_GLOBAL_TX.borrow(cs).replace(Some(tx));
    });
}

// ----------------- Ring buffer -----------------
const PACKET_QUEUE_SIZE: usize = 16;
const EMPTY_PACKET: CANPacket = CANPacket {
    data: [0; MAX_SIZE_PACKET_DATA as usize],
    id: 0,
    dataSize: 0,
    rtr: false,
    extendedID: false,
};

static mut PACKET_QUEUE: [CANPacket; PACKET_QUEUE_SIZE] = [EMPTY_PACKET; PACKET_QUEUE_SIZE];
static mut QUEUE_HEAD: u8 = 0;
static mut QUEUE_TAIL: u8 = 0;
static mut QUEUE_OVERRUN_FLAG: bool = false;

// ----------------- Helper Functions -----------------
#[inline]
fn increment_index(i: u8) -> u8 {
    let n = i.wrapping_add(1);
    if (n as usize) >= PACKET_QUEUE_SIZE { 0 } else { n }
}

#[inline]
fn queue_is_empty() -> bool {
    unsafe { QUEUE_HEAD == QUEUE_TAIL }
}

// Safe wrapper to push to queue (used by Async Task)
fn push_to_queue(pkt: CANPacket) {
    cm_interrupt::free(|_| {
        unsafe {
            let next = increment_index(QUEUE_HEAD);
            if next == QUEUE_TAIL {
                QUEUE_OVERRUN_FLAG = true;
            } else {
                PACKET_QUEUE[QUEUE_HEAD as usize] = pkt;
                QUEUE_HEAD = next;
            }
        }
    });
}

// Safe wrapper to pop from queue (used by waitPackets)
fn pop_from_queue() -> Option<CANPacket> {
    cm_interrupt::free(|_| {
        unsafe {
            if queue_is_empty() {
                None
            } else {
                let pkt = PACKET_QUEUE[QUEUE_TAIL as usize];
                QUEUE_TAIL = increment_index(QUEUE_TAIL);
                Some(pkt)
            }
        }
    })
}

// ----------------- ASYNC RX TASK -----------------
// This replaces the CAN1_RX0 Interrupt Handler
#[embassy_executor::task]
pub async fn can_rx_task(mut rx: CanRx<'static, 'static, CAN1>) {
    loop {
        match rx.read().await {
            Ok(envelope) => { // 'read' returns a single 'Envelope' object
                let frame = envelope.frame; // Access the frame
                let mut pkt = EMPTY_PACKET;
                
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

                pkt.rtr = frame.is_remote_frame();
                
                if let Some(data) = frame.data() {
                    // Data implements Deref<Target=[u8]>, so we can get its length
                    let len = core::cmp::min(data.len(), 8); 
                    pkt.dataSize = len as u8;
                    pkt.data[..len].copy_from_slice(&data[..len]);
                } else {
                    pkt.dataSize = 0;
                }

                push_to_queue(pkt);
            }
            Err(_) => {}
        }
    }
}


// ----------------- Legacy C Interop -----------------
static mut NODE_ID: i32 = 0;
static MATCHER: [unsafe extern "C" fn(u32, u32) -> bool; 3] = [exact, matchID, matchFunction];

#[no_mangle]
pub extern "C" fn pecan_CanInit(config: pecanInit) {
    unsafe {
        NODE_ID = config.nodeId;
        sendStatusUpdate(0, NODE_ID as u32);
    }
}

#[no_mangle]
pub extern "C" fn waitPackets(plpc: *mut PCANListenParamsCollection) -> i16 {
    if plpc.is_null() { return ffi::PCAN_ERR_NOT_RECEIVED as i16; }

    // Use our safe popper
    let mut recv = match pop_from_queue() {
        Some(p) => p,
        None => return ffi::PCAN_ERR_NOT_RECEIVED as i16,
    };

    // --- (Logic below matches your original code exactly) ---
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

    unsafe {
        let coll = &mut *plpc;
        for i in 0..coll.size {
            let clp = &coll.arr[i as usize];
            let mt_idx = clp.mt as i32;
            if mt_idx >= 0 && (mt_idx as usize) < MATCHER.len() {
                let matcher = MATCHER[mt_idx as usize];
                if matcher(recv.id as u32, clp.listen_id) {
                    if let Some(handler) = clp.handler {
                        return handler(&mut recv as *mut CANPacket);
                    }
                }
            }
        }
        if let Some(default_handler) = coll.defaultHandler {
            return default_handler(&mut recv as *mut CANPacket);
        }
    }

    ffi::PCAN_ERR_NOT_RECEIVED as i16
}

#[no_mangle]
pub extern "C" fn sendPacket(p: *mut CANPacket) {
    if p.is_null() { return; }
    let pkt = unsafe { &mut *p };

    let id_raw = pkt.id as u32;
    
    // Create the ID based on the extendedID flag
    let id = if pkt.extendedID {
        Id::Extended(ExtendedId::new(id_raw & 0x1FFF_FFFF).unwrap())
    } else {
        Id::Standard(StandardId::new((id_raw & 0x7FF) as u16).unwrap())
    };
    
    // Then, inside sendPacket:
    let frame = if pkt.rtr {
        Frame::new_remote(id, pkt.dataSize)
    } else {
        let len = core::cmp::min(pkt.dataSize as usize, 8);
        // This is the "Nice" way:
        // It creates a Data object from a slice. It's still 100% stack-based.
        let data = Data::new(&pkt.data[..len]).unwrap(); 
        Frame::new_data(id, data)
    };
    
    // Async block_on behavior for C compatibility
    cm_interrupt::free(|cs| {
        let mut tx_ref = PECAN_GLOBAL_TX.borrow(cs).borrow_mut();
        if let Some(tx) = tx_ref.as_mut() {
             // We use `try_write` or blocking write here. 
             // Since this is extern C and synchronous, we can't await. 
             // We block until space is available.
             loop {
                 match tx.try_write(&frame) {
                     Ok(_) => break, // Success
                     Err(embassy_stm32::can::TryWriteError::Full) => {
                         // Busy wait if full (mimics original behavior)
                         cortex_m::asm::nop(); 
                     }
                 }
                 // Embassy handles bus off errors internally if auto-management is on
             }
        }
    });
}