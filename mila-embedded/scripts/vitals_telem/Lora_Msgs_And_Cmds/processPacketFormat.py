import math
from typing import Any

from Lora_Msgs_And_Cmds.packetFormat import PACK_MINIMUM_BITS_PLUS_8, vitals_to_telem, telem_to_vitals, setPacketParameters, PACK_MINIMUM_BITS, get_maxDataCntBits, get_maxFrameCntBits
from config.parseFile import ParsedFields, globalDefine, Node

def packMaskBits(msg: dict[str, Any], fields: list[Any]) -> None:
    is_plus_8 = msg.get("mask_bits") is PACK_MINIMUM_BITS_PLUS_8
    if msg.get("mask_bits") is PACK_MINIMUM_BITS or is_plus_8:
        bits_sum = sum(f.bits for f in fields)
        if bits_sum % 8 == 0:
            msg["mask_bits"] = 8
        else:
            total_bits_padded = math.ceil(bits_sum / 8) * 8
            msg["mask_bits"] = total_bits_padded - bits_sum
        
        if is_plus_8:
            msg["mask_bits"] += 8


def preprocess_packet_list(packet_list: list[dict], fields_info: ParsedFields, packet_direction: str) -> None:
    for msg in packet_list:
        #Infer name of the packet if not present, and first entry is an enum, use the enum name as the name
        if "name" not in msg and "msgFields" in msg and len(msg["msgFields"]) > 0:
            first_field = msg["msgFields"][0]
            if getattr(first_field, 'isEnum', False):
                msg["name"] = first_field.name
            else:
                raise ValueError(f"Message in telem_to_vitals is missing a 'name' and cannot be inferred: {msg}")
        elif "name" not in msg:
            raise ValueError(f"Message in telem_to_vitals is missing a 'name': {msg}")
        ##

        #Parse the msgFields
        fields = msg.get("msgFields", [])
        for field in fields:
            #call callback functions for values set through callbacks
            for attr in ("bits", "min", "max"):
                val = getattr(field, attr, None)
                if callable(val):
                    setattr(field, attr, val())

            #resolve enums used as field Name
            enum_spec = getattr(field, 'enum', None)
            if enum_spec:
                enum_name = field.name if enum_spec is True else enum_spec
                try:
                    enum = next(entry for entry in fields_info.globalEnums if entry.enum_name == enum_name)
                except StopIteration:
                    raise ValueError(
                        f"Enum '{enum_name}' for field '{field.name}' "
                        f"({packet_direction}) not found in globalEnums"
                    )
                field.min = min(entry.value_int for entry in enum.entries)
                field.max = max(entry.value_int for entry in enum.entries)
                field.bits = max(1, math.ceil(math.log2(int(field.max) + 1)))
            if getattr(field, 'max', None) is None:
                field.max = (1 << field.bits) - 1

        #decide how many bits to make the mask
        packMaskBits(msg, fields)


def preprocess_packets(nodes: list[Node], fields_info: ParsedFields) -> None:
    """
    Resolves all dynamic values in the packet format definitions.
    This includes callables, enums, and PACK_MINIMUM_BITS.
    This function modifies the vitals_to_telem and telem_to_vitals lists in-place.
    """
    setPacketParameters(nodes, fields_info.maxFrameCount, fields_info.maxDataCount, fields_info.globalEnums)

    #Manually add additional globalDefines based on vitals<->telem packet formatting
    nodeCount, maxFrameCntBits, maxDataCntBits = len(nodes), get_maxFrameCntBits(), get_maxDataCntBits()
    fields_info.globalDefines.append(globalDefine("nodeCount", str(nodeCount), nodeCount))
    fields_info.globalDefines.append(globalDefine("maxFrameCntBits", str(maxFrameCntBits), maxFrameCntBits))
    fields_info.globalDefines.append(globalDefine("maxDataInFrameBits", str(maxDataCntBits), maxDataCntBits))

    # --- Pre-process vitals_to_telem ---
    preprocess_packet_list(vitals_to_telem, fields_info, "vitals_to_telem")

    # --- Pre-process telem_to_vitals ---
    preprocess_packet_list(telem_to_vitals, fields_info, "telem_to_vitals")