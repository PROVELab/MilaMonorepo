import os
from typing import Any

def createSensorCommandInfrastructure(sensor_commands: list[dict[str, Any]],
                                      sub_dir_path: str,
                                      sensor_helper_include: str = "../../common/sensorHelper.hpp",
                                      pecan_include: str = "../../../pecan/pecan.h",
                                      helper_subdir: str = "helper",
                                      generate_callback_stubs: bool = True) -> None:
    if not sensor_commands:
        return

    helper_dir = os.path.join(sub_dir_path, helper_subdir)
    os.makedirs(helper_dir, exist_ok=True)

    with open(os.path.join(helper_dir, "sensorRecvLUT.cpp"), 'w') as f:
        f.write('#include "myDefines.hpp"\n#include <string.h>\n ')
        f.write(f'#include "{sensor_helper_include}"\n')
        f.write(f'#include "{pecan_include}"\n\nextern "C" {{\n\n')

        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            if fields:
                f.write(f"const simpleDataPoint {name}_fields[{len(fields)}] = {{\n")
                for field in fields:
                    f.write(f"    {{ {field.min}, {field.max}, {field.bits} }},\n")
                f.write("};\n\n")

        for msg in sensor_commands:
            name = msg["name"]
            fields = msg.get("msgFields", [])
            contains_payload = msg.get("containsPayload", False)
            struct_name = f"{name}_args_t"
            f.write(f"static void {name}_wrapper(const uint8_t* raw_packet, size_t packet_len, int8_t* bitIndex) {{\n")
            has_struct = len(fields) > 0 or contains_payload
            if has_struct:
                f.write("    union {\n")
                f.write(f"        {struct_name} s;\n")
                if fields:
                    f.write(f"        int32_t data_arr[{len(fields)}];\n")
                f.write("    } u __attribute__((aligned(4)));\n\n")

                if fields:
                    f.write("    uint8_t unpack_packet[8] = {0};\n")
                    f.write("    if (packet_len > sizeof(unpack_packet)) { return; }\n")
                    f.write("    memcpy(unpack_packet, raw_packet, packet_len);\n")
                    f.write(f"    for (int i = 0; i < {len(fields)}; ++i) {{\n")
                    f.write(f"        pecan_unpack(&u.data_arr[i], &unpack_packet, &{name}_fields[i], bitIndex);\n")
                    f.write("    }\n")

                if contains_payload:
                    f.write("    size_t fixed_bytes = (*bitIndex + 7) / 8;\n")
                    f.write("    if (packet_len > fixed_bytes) { u.s.payload = raw_packet + fixed_bytes; u.s.max_payload_size = packet_len - fixed_bytes; }")
                    f.write(" else { u.s.payload = NULL; u.s.max_payload_size = 0; }\n")

                f.write(f"    on{name}(u.s);\n")
            else:
                f.write(f"    on{name}();\n")
            f.write("}\n\n")

        f.write("const SensorRecvPacketLUTEntry sensorRecvPacketLUT[] = {\n")
        for msg in sensor_commands:
            f.write(f"    {{ // {msg['name']}\n")
            f.write(f"        .fields = {'NULL' if not msg.get('msgFields') else msg['name']+'_fields'},\n")
            f.write(f"        .num_fields = {len(msg.get('msgFields',[]))},\n")
            f.write(f"        .packetIsCustom = {str(msg.get('containsPayload', False)).lower()},\n")
            f.write(f"        .callback_wrapper = {msg['name']}_wrapper,\n    }},\n")
        f.write("};\n")
        f.write("const size_t sensorRecvPacketLUTSize = sizeof(sensorRecvPacketLUT) / sizeof(SensorRecvPacketLUTEntry);\n")
        f.write("\n} // extern C\n")

    callbacks_path = os.path.join(helper_dir, "sensorRecvCallbacks.cpp")
    if generate_callback_stubs and not os.path.exists(callbacks_path):
        with open(callbacks_path, 'w') as f:
            f.write("/**\n")
            f.write(" * @file sensorRecvCallbacks.cpp\n")
            f.write(" * @brief Skeleton implementations for command callbacks.\n")
            f.write(" * NOTE: You may move these implementations to your main.c/cpp file for convenience.\n")
            f.write(" */\n\n")
            f.write('#include "myDefines.hpp"\n\n')
            for msg in sensor_commands:
                name = msg["name"]
                fields = msg.get("msgFields", [])
                contains_payload = msg.get("containsPayload", False)
                struct_name = f"{name}_args_t"
                has_struct = len(fields) > 0 or contains_payload
                param_str = f"{struct_name} args" if has_struct else "void"
                f.write(f"void on{name}({param_str}) {{\n    // TODO: Implement logic for {name}\n}}\n\n")
