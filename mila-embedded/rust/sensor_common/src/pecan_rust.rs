#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use core::cell::RefCell;
use core::ffi::{c_char, CStr};

use cortex_m::interrupt as cm_interrupt;
use cortex_m::interrupt::Mutex;
use embassy_executor::Spawner;
use embassy_stm32::bind_interrupts;
use embassy_stm32::can::bxcan::filter::Mask32;
use embassy_stm32::can::bxcan::{Data, ExtendedId, Fifo, Frame, Id, StandardId};
use embassy_stm32::can::{
    Can, CanRx, CanTx, Rx0InterruptHandler, Rx1InterruptHandler, SceInterruptHandler,
    TxInterruptHandler,
};
use embassy_stm32::peripherals::CAN1;
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::channel::Channel;
use embassy_time::Duration;
use rtt_target::{rprint, rprintln};
use static_cell::StaticCell;

use crate::ffi::{
    exact, matchFunction, matchID, pecanInit, sendStatusUpdate, CANListenParam, CANPacket,
    PCANListenParamsCollection, MAX_SIZE_PACKET_DATA, PCAN_ERR_NOT_RECEIVED,
};

bind_interrupts!(struct Irqs {
    CAN1_RX0 => Rx0InterruptHandler<CAN1>;
    CAN1_RX1 => Rx1InterruptHandler<CAN1>;
    CAN1_TX => TxInterruptHandler<CAN1>;
    CAN1_SCE => SceInterruptHandler<CAN1>;
});

pub static PECAN_GLOBAL_TX: Mutex<RefCell<Option<CanTx<'static, 'static, CAN1>>>> =
    Mutex::new(RefCell::new(None));
static RX_CHANNEL: Channel<CriticalSectionRawMutex, CANPacket, 16> = Channel::new();
static CAN_INSTANCE: StaticCell<Can<'static, CAN1>> = StaticCell::new();
static MATCHER: [unsafe extern "C" fn(u32, u32) -> bool; 3] = [exact, matchID, matchFunction];

pub static mut NODE_ID: i32 = 0;

fn pecan_CanInit_tx(tx: CanTx<'static, 'static, CAN1>) {
    cm_interrupt::free(|cs| {
        PECAN_GLOBAL_TX.borrow(cs).replace(Some(tx));
    });
}

#[embassy_executor::task]
async fn can_rx_task(mut rx: CanRx<'static, 'static, CAN1>) {
    loop {
        match rx.read().await {
            Ok(envelope) => {
                let frame = envelope.frame;
                let mut pkt = CANPacket {
                    data: [0; MAX_SIZE_PACKET_DATA as usize],
                    id: 0,
                    dataSize: 0,
                    rtr: false,
                    extendedID: false,
                };

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
                    let len = core::cmp::min(data.len(), 8);
                    pkt.dataSize = len as u8;
                    pkt.data[..len].copy_from_slice(&data.as_ref()[..len]);
                }

                if RX_CHANNEL.try_send(pkt).is_err() {
                    rprintln!("RX QUEUE OVERRUN - Dropping packet!");
                }
            }
            Err(_) => {
                rprintln!("CAN RX Error (Bus Off / Stuffing Error)");
            }
        }
    }
}

pub async fn pecan_CanInit(
    config: pecanInit,
    spawner: &Spawner,
    can1: embassy_stm32::peripherals::CAN1,
    rx_pin: embassy_stm32::peripherals::PA11,
    tx_pin: embassy_stm32::peripherals::PA12,
) {
    let mut can = Can::new(can1, rx_pin, tx_pin, Irqs);

    can.as_mut()
        .modify_filters()
        .enable_bank(0, Fifo::Fifo0, Mask32::accept_all());
    can.as_mut()
        .modify_config()
        .set_loopback(false)
        .leave_disabled();
    can.set_bitrate(500_000);
    can.enable().await;

    let can_static = CAN_INSTANCE.init(can);
    let (tx, rx) = can_static.split();

    pecan_CanInit_tx(tx);
    spawner.spawn(can_rx_task(rx)).unwrap();

    unsafe {
        NODE_ID = config.nodeId;
        sendStatusUpdate(0, NODE_ID as u32);
    }
    rprintln!("CAN System Online: Filters Open, 500kbps, Normal Mode");
}

#[no_mangle]
pub extern "C" fn flexiblePrint(str_ptr: *const c_char) {
    if str_ptr.is_null() {
        return;
    }

    let Ok(text) = core::str::from_utf8(unsafe { CStr::from_ptr(str_ptr) }.to_bytes()) else {
        rprint!("<non-utf8>");
        return;
    };

    rprint!("{}", text);
}

#[no_mangle]
pub extern "C" fn sendPacket(p: *mut CANPacket) {
    if p.is_null() {
        return;
    }
    let pkt = unsafe { &mut *p };

    let id_raw = pkt.id as u32;
    let id = if pkt.extendedID {
        Id::Extended(ExtendedId::new(id_raw & 0x1fff_ffff).unwrap())
    } else {
        Id::Standard(StandardId::new((id_raw & 0x7ff) as u16).unwrap())
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
                if tx.try_write(&frame).is_ok() {
                    success = true;
                }
            }
        });

        if !initialized {
            rprintln!("TX ERROR: Driver not initialized!");
            return;
        }

        if success {
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

#[no_mangle]
pub extern "C" fn waitPackets(plpc: *mut PCANListenParamsCollection) -> i16 {
    if plpc.is_null() {
        return PCAN_ERR_NOT_RECEIVED as i16;
    }

    let mut recv = match RX_CHANNEL.try_receive() {
        Ok(packet) => packet,
        Err(_) => return PCAN_ERR_NOT_RECEIVED as i16,
    };

    if recv.extendedID {
        recv.id &= 0x1fff_ffff;
    } else {
        recv.id &= 0x7ff;
    }

    if recv.rtr {
        recv.dataSize = 0;
    } else {
        let sz = recv.dataSize as usize;
        let clamped = core::cmp::min(sz, MAX_SIZE_PACKET_DATA as usize);
        recv.dataSize = clamped as u8;
    }

    let used = recv.dataSize as usize;
    if used < MAX_SIZE_PACKET_DATA as usize {
        for b in &mut recv.data[used..MAX_SIZE_PACKET_DATA as usize] {
            *b = 0;
        }
    }

    unsafe {
        let coll = &mut *plpc;
        for i in 0..coll.size {
            let clp: &CANListenParam = &coll.arr[i as usize];
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

    PCAN_ERR_NOT_RECEIVED as i16
}
