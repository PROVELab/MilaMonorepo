import math
import os
from typing import Any, TextIO

from Lora_Msgs_And_Cmds.packetFormat import vitals_to_telem, telem_to_vitals
from config.parseFile import expression_to_int, ACCESS

def reverse_bits(n: int, length: int) -> int:
    """Reverses the bottom `length` bits of an integer `n`."""
    """if n is 0b01001, and lengtg=3, returns 0b01100 as an integer"""
    result = 0
    for i in range(length):
        if (n >> i) & 1:
            result |= 1 << (length - 1 - i)
    return result

def _assign_prefix_free_masks(
    packet_list: list[dict[str, Any]],
    map_file: TextIO,
    packet_direction_name: str,
) -> None:
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
    current_len = sorted_msgs[0].get("mask_bits", 0)
    
    for msg in sorted_msgs:
        mask_len = msg["mask_bits"]
        if mask_len > current_len:
            #reduce the relative significance of the bit used on code by how much mask size changes
            next_code <<= (mask_len - current_len) 
            current_len = mask_len
        if len(bin(next_code)[2:]) > mask_len:
            raise ValueError(f"Not enough mask space for all {packet_direction_name} packets. Failed at packet '{msg['name']}'. Ran out of codes of length {mask_len}.")
        
        #convert from most significant bit prefix free (easier to generate like this, bc can just shift left for more room) 
        # to least significant bit prefix, so that its parse-able, with bit0 parsed first
        reversed_code = reverse_bits(next_code, mask_len)
        msg["mask"] = reversed_code
        
        map_file.write(f"{msg['name']} ({mask_len} bits): {reversed_code} (0b{reversed_code:0{mask_len}b}) "
                       f"<- reversed from canonical {next_code} (0b{next_code:0{mask_len}b})\n")
        next_code += 1 #the bottom most bit of code is used on each iteration

def _assign_can_forwarding_masks(
    packet_list: list[dict[str, Any]],
    nodes: list[Any],
) -> None:
    """
    Assigns simple sequential masks for commands that are forwarded over CAN.
    Groups packets by 'targetNode' and assigns a unique mask within each group.
    This modifies the packet dictionaries in-place.
    """
    node_map_by_name = {node.name: node for node in nodes}
    node_map_by_id = {node.node_id: node for node in nodes}

    ##-----------Create a lookup of the different nodes that packets are forwarded to
    forwarded_packets_by_node = {}
    for msg in packet_list:
        target_node_str = msg.get("targetNode")
        if not target_node_str:
            raise ValueError(f"targetNode is not known for msg: {msg}")        
        # "vitals" is a valid target. do not try to map to a sensor ID
        if target_node_str == "vitals": 
            continue

        # 2. If it's not a known name, try the sensor name as an integer ID
        if target_node_str not in node_map_by_name:
            try:
                target_id = expression_to_int(target_node_str)
                # Check if the id exists
                if target_id not in node_map_by_id:
                    raise ValueError(f"Mapped ID {target_id} (from '{target_node_str}') does not exist in nodes.")
                #if so, assign the id to the node
                target_node_str = node_map_by_id[target_id].name

            except (ValueError, NameError) as e:
                # Catch expression errors and abort with context
                raise ValueError(f"Could not map target '{target_node_str}' to a valid node.") from e
            
        forwarded_packets_by_node.setdefault(target_node_str, []).append(msg)
    ##-----------

    #Set the mask bits of each msg based on how many msgs are going to that node
    for msgs in forwarded_packets_by_node.values():
        if not msgs:
            continue # Skip empty lists safely
            
        can_mask_bits = math.ceil(math.log2(len(msgs)))
        for i, msg in enumerate(msgs):
            msg['can_mask'] = i
            msg['can_mask_bits'] = can_mask_bits


def assign_all_masks(generated_code_dir: str, nodes: list[Any]) -> None:
    """
    Assigns all masks for both CAN forwarding and telemetry packet identification.
    This includes CAN-level masks for forwarded packets and prefix-free masks for telemetry packets.
    """
    _assign_can_forwarding_masks(telem_to_vitals, nodes)
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
        # Add packet_idx for consistency, though it's not currently used by the C code generator
        for i, packet in enumerate(telem_to_vitals):
            packet['packet_idx'] = i
        _assign_prefix_free_masks(telem_to_vitals, map_file, "telem-to-vitals")
