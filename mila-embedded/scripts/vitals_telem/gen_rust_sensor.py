from config.parseFile import dataPoint_fields, CANFrame_fields, ACCESS, expression_to_int
from Lora_Msgs_And_Cmds.packetFormat import CUSTOM

def generate_rust_main(f, node, dataNames, numDataForNode, base_dir, has_commands, sensor_commands):
    collector_funcs = ""
    collector_names = []
    frame_details = []
    current_data_idx = 0
    
    # 1. Generate collector functions and gather frame info
    for frame in ACCESS(node, "CANFrames")["value"]:
        frame_details.append({ "freq": ACCESS(frame, "frequency")["value"], "num_data": ACCESS(frame, "numData")["value"], "start_idx": current_data_idx })
        for dataPoint in ACCESS(frame, "dataInfo")["value"]:
            data_name = dataNames[current_data_idx]
            start_val = str(ACCESS(dataPoint, "startingValue")["value"])
            
            # Create the exact same function name expected by the C header
            func_name = f"collect_{data_name}"
            collector_names.append(func_name)
            
            # Generate the Rust function definition
            collector_funcs += f"#[allow(unused_variables)]\n"
            collector_funcs += f"fn {func_name}(cancel_frame_send: &mut bool) -> i32 {{\n"
            collector_funcs += f"    // rprintln!(\"collecting {data_name}\");\n"
            collector_funcs += f"    {start_val}\n"
            collector_funcs += f"}}\n\n"
            
            current_data_idx += 1

    # 2. Generate periodic sender tasks for each frame
    sender_tasks = ""
    for i, frame in enumerate(frame_details):
        sender_tasks += f"""
#[embassy_executor::task]
async fn send_frame_{i}_task() {{
    let mut ticker = embassy_time::Ticker::every(embassy_time::Duration::from_millis({frame['freq']}));
    loop {{
        let mut temp_data: [u8; 8] = [0; 8];
        let mut curr_bit: i8 = 0;
        let mut send_frame = true;

        // Collect and pack data
"""
        for j in range(frame['num_data']):
            collector_idx = frame['start_idx'] + j
            collector_name = collector_names[collector_idx]
            sender_tasks += f"        let data = {collector_name}(&mut send_frame);\n"
            sender_tasks += f"        if !send_frame {{ continue; }} // Skip frame send if requested by a collector\n"
            sender_tasks += f"        unsafe {{ pecan_pack(temp_data.as_mut_ptr(), &mut curr_bit, data, &myframes[{i}].dataInfo.add({j})); }}\n"

        sender_tasks += f"""
        // Send the packet
        if curr_bit > 0 {{
            let mut data_packet: CANPacket = unsafe {{ core::mem::zeroed() }};
            data_packet.extendedID = 1;
            data_packet.id = unsafe {{ combinedIDExtended(functionCodes_transmitData as u32, myId as u32, {i} as u32) }};
            let data_size = ((7 + curr_bit) / 8) as i8;
            
            unsafe {{
                writeData(&mut data_packet, temp_data.as_ptr() as *mut i8, data_size);
                sendPacket(&mut data_packet);
            }}
        }}

        ticker.next().await;
    }}
}}
"""

    # 3. Generate command handling code (if any)
    command_handler_code = ""
    receive_task_code = ""
    init_plpc_code = ""
    init_receiver_code = ""

    if has_commands:
        on_command_funcs = ""
        for cmd in sensor_commands:
            on_command_funcs += f"fn on_{cmd['name']}(_args: bindings::{cmd['name']}_args_t) {{\n"
            on_command_funcs += f"    // TODO: Implement logic for {cmd['name']}\n"
            on_command_funcs += "}\n\n"

        mask_bits = sensor_commands[0].get('can_mask_bits', 0)

        dispatch_logic = ""
        if mask_bits > 0:
            dispatch_logic += "    let mut mask_val: i32 = 0;\n"
            dispatch_logic += f"    let mask_field = bindings::simpleDataPoint {{ bits: {mask_bits}, min: 0, max: (1 << {mask_bits}) - 1 }};\n"
            dispatch_logic += "    bindings::pecan_unpack(&mut mask_val, p.cast::<u8>(), &mask_field, &mut bit_index);\n\n"
            dispatch_logic += "    match mask_val {\n"
            for cmd in sensor_commands:
                mask = cmd['can_mask']
                dispatch_logic += f"        {mask} => {{ // {cmd['name']}\n"
                dispatch_logic += f"            let mut args: bindings::{cmd['name']}_args_t = unsafe {{ core::mem::zeroed() }};\n"
                dispatch_logic += f"            let fields = bindings::{cmd['name']}_fields;\n"
                dispatch_logic += f"            let data_arr = &mut args as *mut _ as *mut i32;\n"
                dispatch_logic += f"            for i in 0..{len(cmd.get('msgFields', []))} {{\n"
                dispatch_logic += "                bindings::pecan_unpack(data_arr.add(i), p.cast::<u8>(), &fields[i], &mut bit_index);\n"
                dispatch_logic += "            }\n"
                dispatch_logic += f"            on_{cmd['name']}(args);\n"
                dispatch_logic += "        }},\n"
            dispatch_logic += "        _ => { /* Invalid mask */ }\n"
            dispatch_logic += "    }\n"
        else: # Only one command
            cmd = sensor_commands[0]
            dispatch_logic += f"    let mut args: bindings::{cmd['name']}_args_t = unsafe {{ core::mem::zeroed() }};\n"
            dispatch_logic += f"    let fields = bindings::{cmd['name']}_fields;\n"
            dispatch_logic += f"    let data_arr = &mut args as *mut _ as *mut i32;\n"
            dispatch_logic += f"    for i in 0..{len(cmd.get('msgFields', []))} {{\n"
            dispatch_logic += "        bindings::pecan_unpack(data_arr.add(i), p.cast::<u8>(), &fields[i], &mut bit_index);\n"
            dispatch_logic += "    }\n"
            dispatch_logic += f"    on_{cmd['name']}(args);\n"

        command_handler_code = f"""
// --- Command Handlers ---
{on_command_funcs}
#[no_mangle] // This function must be callable from C
pub unsafe extern "C" fn handle_telemetry_command(p_packet: *mut bindings::CANPacket) -> i16 {{
    let p = (*p_packet).data.as_ptr();
    let mut bit_index: i8 = 0;
{dispatch_logic}
    0
}}

fn add_command_handler(plpc: &mut bindings::PCANListenParamsCollection) {{
    let mut telem_command_param: bindings::CANListenParam = unsafe {{ core::mem::zeroed() }};
    telem_command_param.listen_id = unsafe {{ bindings::combinedID(bindings::functionCodes_TelemetryCommand as u32, myId as u32) }};
    telem_command_param.handler = Some(handle_telemetry_command);
    telem_command_param.mt = bindings::mt_MATCH_EXACT;
    unsafe {{ bindings::addParam(plpc, telem_command_param) }};
}}
"""
        init_plpc_code = """
    let mut plpc: core::mem::MaybeUninit<PCANListenParamsCollection> = core::mem::MaybeUninit::zeroed();
    let mut plpc = unsafe { plpc.assume_init() };

    plpc.defaultHandler = Some(defaultPacketRecv);
    plpc.size = 0;

    add_command_handler(&mut plpc);
"""
        init_receiver_code = "    spawner.spawn(receive_msg_task(plpc)).unwrap();"
        receive_task_code = """
// --- Receive Message Task ---
#[embassy_executor::task]
async fn receive_msg_task(mut plpc: PCANListenParamsCollection) {
    loop {
        unsafe {
            while waitPackets(&mut plpc) != bindings::status_NOT_RECEIVED {
                // Packet handled by callback
            }
        }
        Timer::after(Duration::from_millis(10)).await;
    }
}
"""

    # 4. Assemble the final file content
    content = f"""//! Sensor main module, generated by scripts/gen_rust_sensor.py
//! This file should be included in your main.rs and the init_sensor function should be called.

use embassy_executor::Spawner;
use embassy_time::{{Duration, Timer}};
use rtt_target::rprintln;

// Include bindgen definitions
#[allow(non_upper_case_globals, non_camel_case_types, non_snake_case)]
pub mod bindings {{
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}}
use bindings::*; // Allow access to C functions and types

// --- Data Collectors ---
{collector_funcs}

// --- Frame Sender Tasks ---
{sender_tasks}

// --- Command Handling ---
{command_handler_code}
{receive_task_code}

// --- Main Sensor Initialization ---
pub fn init_sensor(spawner: &Spawner) {{
    rprintln!("Initializing sensor tasks for node {{}}...", myId);

    // Spawn sender tasks
{ "".join([f"    spawner.spawn(send_frame_{i}_task()).unwrap();\\n" for i, _ in enumerate(frame_details)]) }
    // Initialize and spawn receiver task if there are commands
    {init_plpc_code}
    {init_receiver_code}
}}
"""
    # Write directly to the file object passed in
    f.write(content)