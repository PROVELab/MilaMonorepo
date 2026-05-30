import math
from packetFormat import PACK_MINIMUM_BITS_PLUS_8, vitals_to_telem, telem_to_vitals, setPacketParameters, PACK_MINIMUM_BITS, get_maxDataCntBits, get_maxFrameCntBits
from parseFile import globalEnums, globalDefines, globalDefine

def packMaskBits(msg, fields):
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

def preprocess_packets(nodes, maxFrameCnt, maxDataCnt):
    """
    Resolves all dynamic values in the packet format definitions.
    This includes callables, enums, and PACK_MINIMUM_BITS.
    This function modifies the vitals_to_telem and telem_to_vitals lists in-place.
    """
    setPacketParameters(nodes, maxFrameCnt, maxDataCnt)
    nodeCount = len(nodes)

    globalDefines.append(globalDefine("nodeCount", nodeCount, nodeCount))
    globalDefines.append(globalDefine("maxFrameCntBits", get_maxFrameCntBits(), get_maxFrameCntBits()))
    globalDefines.append(globalDefine("maxDataInFrameBits", get_maxDataCntBits(), get_maxDataCntBits()))

    # --- Pre-process vitals_to_telem ---
    for msg in vitals_to_telem:
        fields = msg.get("msgFields", [])
        for field in fields:
            for attr in ("bits", "min", "max"):
                val = getattr(field, attr, None)
                if callable(val):
                    setattr(field, attr, val())
            enum_spec = getattr(field, 'enum', None)
            if enum_spec:
                enum_name = field.name if enum_spec is True else enum_spec
                try:
                    enum = next(entry for entry in globalEnums if entry.enum_name == enum_name)
                except StopIteration:
                    raise ValueError(f"Enum '{enum_name}' for field '{field.name}' not found in globalEnums")
                field.min = min(entry.value_int for entry in enum.entries)
                field.max = max(entry.value_int for entry in enum.entries)
                field.bits = max(1, math.ceil(math.log2(int(field.max) + 1)))
            if getattr(field, 'max', None) is None:
                field.max = (1 << field.bits) - 1
        packMaskBits(msg, fields)


    # --- Pre-process telem_to_vitals ---
    # Fix typo 'targetNode:' -> 'targetNode' and infer names
    for msg in telem_to_vitals:
        if "targetNode:" in msg:
            msg["targetNode"] = msg.pop("targetNode:")
        if "name" not in msg and "msgFields" in msg and len(msg["msgFields"]) > 0:
            first_field = msg["msgFields"][0]
            if getattr(first_field, 'isEnum', False):
                msg["name"] = first_field.name
            else:
                raise ValueError(f"Message in telem_to_vitals is missing a 'name' and cannot be inferred: {msg}")
        elif "name" not in msg:
             raise ValueError(f"Message in telem_to_vitals is missing a 'name': {msg}")

    for msg in telem_to_vitals:
        fields = msg.get("msgFields", [])
        for field in fields:
            for attr in ("bits", "min", "max"):
                val = getattr(field, attr, None)
                if callable(val):
                    setattr(field, attr, val())
            enum_spec = getattr(field, 'enum', None)
            if enum_spec:
                enum_name = field.name if enum_spec is True else enum_spec
                try:
                    enum = next(entry for entry in globalEnums if entry.enum_name == enum_name)
                except StopIteration:
                    raise ValueError(f"Enum '{enum_name}' for field '{field.name}' (telem_to_vitals) not found. Please define it in your .def file.")
                field.min = min(entry.value_int for entry in enum.entries)
                field.max = max(entry.value_int for entry in enum.entries)
                field.bits = max(1, math.ceil(math.log2(int(field.max) + 1)))
            if getattr(field, 'max', None) is None:
                field.max = (1 << field.bits) - 1

        packMaskBits(msg, fields)
    
