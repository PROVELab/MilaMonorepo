import math
import os
from Lora_Msgs_And_Cmds.packetFormat import vitals_to_telem, telem_to_vitals

def reverse_bits(n, length):
    """Reverses the bottom `length` bits of an integer `n`."""
    result = 0
    for i in range(length):
        if (n >> i) & 1:
            result |= 1 << (length - 1 - i)
    return result

def _assign_prefix_free_masks(packet_list, map_file, packet_direction_name):
    """
    Assigns unique, prefix-free masks to a list of packets using a canonical
    Huffman-like algorithm and writes the assignments to a mapping file.
    This function modifies the packet dictionaries in-place by adding "mask" keys.
    The generated codes are bit-reversed to be LSB-prefix-free (suffix-free).
    """
    if any(msg.get("mask_bits", 0) == 0 for msg in packet_list):
        unmasked_names = [m['name'] for m in packet_list if m.get("mask_bits", 0) == 0]
        raise ValueError(f"FATAL: The following {packet_direction_name} packets have a 0-bit mask, which is not allowed: {unmasked_names}")

    sorted_msgs = sorted(packet_list, key=lambda m: (m.get("mask_bits", 0), m['name']))

    next_code = 0
    current_len = 0
    if sorted_msgs:
        current_len = sorted_msgs[0].get("mask_bits", 0)

    for msg in sorted_msgs:
        mask_len = msg.get("mask_bits", 0)
        if mask_len > current_len:
            next_code <<= (mask_len - current_len)
            current_len = mask_len
        if len(bin(next_code)[2:]) > mask_len:
            raise ValueError(f"Not enough mask space for all {packet_direction_name} packets. Failed at packet '{msg['name']}'. Ran out of codes of length {mask_len}.")
        
        reversed_code = reverse_bits(next_code, mask_len)
        msg["mask"] = reversed_code
        
        map_file.write(f"{msg['name']} ({mask_len} bits): {reversed_code} (0b{reversed_code:0{mask_len}b}) "
                       f"<- reversed from canonical {next_code} (0b{next_code:0{mask_len}b})\n")
        next_code += 1

def _assign_can_forwarding_masks(packet_list):
    """
    Assigns simple sequential masks for commands that are forwarded over CAN.
    Groups packets by 'targetNode' and assigns a unique mask within each group.
    This modifies the packet dictionaries in-place.
    """
    forwarded_packets_by_node = {}
    for msg in packet_list:
        target_node = msg.get("targetNode")
        if target_node and target_node != "vitals":
            if target_node not in forwarded_packets_by_node:
                forwarded_packets_by_node[target_node] = []
            forwarded_packets_by_node[target_node].append(msg)

    for node_name, msgs in forwarded_packets_by_node.items():
        if len(msgs) > 1:
            can_mask_bits = math.ceil(math.log2(len(msgs)))
            for i, msg in enumerate(msgs):
                msg['can_mask'] = i
                msg['can_mask_bits'] = can_mask_bits
        else:
            for msg in msgs:
                msg['can_mask'] = 0
                msg['can_mask_bits'] = 0

def assign_all_masks(generated_code_dir):
    """
    Assigns all masks for both CAN forwarding and telemetry packet identification.
    This includes CAN-level masks for forwarded packets and prefix-free masks for telemetry packets.
    """
    _assign_can_forwarding_masks(telem_to_vitals)
    mapping_path = os.path.join(generated_code_dir, "mask_mappings.txt")
    with open(mapping_path, 'w') as map_file:
        map_file.write("--- Vitals to Telem ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")

        # Add packet_idx for Java generator before sorting for mask assignment
        for i, packet in enumerate(vitals_to_telem):
            packet['packet_idx'] = i

        _assign_prefix_free_masks(vitals_to_telem, map_file, "vitals-to-telem")
    
        map_file.write("\n\n--- Telem to Vitals ---\n")
        map_file.write("Packet Name -> Assigned Mask\n\n")
        _assign_prefix_free_masks(telem_to_vitals, map_file, "telem-to-vitals")