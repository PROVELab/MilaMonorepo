from parseFile import dataPoint_fields, CANFrame_fields, ACCESS, expression_to_int


def generate_rust_main(f, node, dataNames, numDataForNode, localDataIndex, base_dir, has_commands):
    collector_funcs = ""
    collector_names = []
    
    current_data_idx = localDataIndex
    
    # Iterate through the frames and data points just like _write_esp_main
    for frame in ACCESS(node, "CANFrames")["value"]:
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            data_name = dataNames[current_data_idx]
            start_val = str(ACCESS(dataPoint, "startingValue")["value"])
            
            # Create the exact same function name expected by the C header
            func_name = f"collect_{data_name}"
            collector_names.append(func_name)
            
            # Generate the Rust function definition
            collector_funcs += f"fn {func_name}(_cancel_frame_send: &mut bool) -> i32 {{\n"
            collector_funcs += f"    // defmt::info!(\"collecting {data_name}\");\n"
            collector_funcs += f"    {start_val}\n"
            collector_funcs += f"}}\n\n"
            
            current_data_idx += 1

    num_collectors = len(collector_names)
    array_elements = ",\n    ".join(collector_names)

    content = f"""#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_time::{{Duration, Timer}};
use panic_halt as _; 

// Include bindgen definitions
#[allow(non_upper_case_globals, non_camel_case_types, non_snake_case)]
pub mod bindings {{
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}}
use bindings::*;

mod sensor_helper;
use sensor_helper::*;

// --- Data Collectors ---
{collector_funcs}
// Static array of collector callbacks
static DATA_COLLECTORS: [fn(&mut bool) -> i32; {num_collectors}] = [
    {array_elements}
];

// --- Receive Message Task ---
#[embassy_executor::task]
async fn receive_msg_task() {{
    let mut plpc: core::mem::MaybeUninit<PCANListenParamsCollection> = core::mem::MaybeUninit::zeroed();
    let mut plpc = unsafe {{ plpc.assume_init() }};

    plpc.defaultHandler = Some(defaultPacketRecv);
    plpc.size = 0;

    loop {{
        unsafe {{
            while waitPackets(&mut plpc) != NOT_RECEIVED {{
                // Handling loop
            }}
        }}
        Timer::after(Duration::from_millis(10)).await;
    }}
}}

// --- Main Application ---
#[embassy_executor::main]
async fn main(spawner: Spawner) {{
    // Initialize CAN via Pecan C-bindings
    unsafe {{
        let config = pecanInit {{
            nodeId: myId as i32,
            pin1: defaultPin as i32, 
            pin2: defaultPin as i32,
        }};
        pecan_CanInit(config);
    }}

    // Spawns background tasks for all defined numFrames 
    sensor_init(&spawner, core::ptr::null_mut(), &DATA_COLLECTORS);

    // Start CAN receive listener
    spawner.spawn(receive_msg_task()).unwrap();

    loop {{
        Timer::after(Duration::from_secs(10)).await;
    }}
}}
"""
    # Write directly to the file object passed in
    f.write(content)