import os
from typing import Any


def generate_single_cpp_callback_skeleton(
    output_path: str,
    packet: dict[str, Any],
) -> None:
    msg = packet
    name = msg["name"]
    fields = msg.get("msgFields", [])
    contains_payload = msg.get("containsPayload", False)
    struct_name = f"{name}_args_t"

    print(f"Generating initial C++ callback skeleton for {name} at {os.path.relpath(output_path)}...")
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, 'w') as f:
        f.write('#include "../vitalsGen/vitalsPacketRecvLUT.h"\n')
        f.write('#include "esp_log.h"\n\n')
        f.write('static const char* TAG = "VitalsRecvCallbacks";\n\n')

        params = []
        has_struct = len(fields) > 0 or contains_payload
        if has_struct:
            params.append(f"{struct_name} args")

        param_str = ", ".join(params) if params else "void"
        return_type = "size_t" if contains_payload else "void"
        f.write(f"{return_type} on{name}({param_str}) {{\n")

        log_args = []
        if fields:
            for field in fields:
                if field.enum is not None and field.enum:
                    log_args.append(f"args.{field.name}.i32")
                else:
                    log_args.append(f"args.{field.name}")

        log_prefix = f"Callback on{name} called."
        target_node = msg.get("targetNode", "vitals")
        if target_node != "vitals":
            log_prefix += f" (packet was forwarded to {target_node} by Vitals)."

        if fields:
            fmt_string = f'"{log_prefix} '
            for i, field in enumerate(fields):
                fmt_string += f'{field.name}: %" PRId32'
                if i < len(fields) - 1:
                    fmt_string += ' ", '
            args_string = ', '.join(log_args)
            f.write(f'    ESP_LOGI(TAG, {fmt_string}, {args_string});\n')
        else:
            f.write(f'    ESP_LOGI(TAG, "{log_prefix}");\n')

        f.write(f"    // TODO: Implement logic for {name}\n")

        if contains_payload:
            f.write("    // Access payload via args.payload, with max size args.max_payload_size\n")
            f.write("    // Example: ESP_LOG_BUFFER_HEX(TAG, args.payload, args.max_payload_size);\n")
            f.write("    return 0; // Return number of payload bytes consumed\n")
        f.write("}\n")


def generate_vitals_callback_skeletons(
    telem_to_vitals: list[dict[str, Any]],
    vitals_callbacks_dir: str,
) -> None:
    print("\n--- Generating Telem->Vitals Callback Skeletons (C++) ---")
    for packet in telem_to_vitals:
        skeleton_path = os.path.join(vitals_callbacks_dir, f"on{packet['name']}.c")
        if not os.path.exists(skeleton_path):
            generate_single_cpp_callback_skeleton(skeleton_path, packet)
