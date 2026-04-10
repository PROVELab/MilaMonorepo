FIXED = object()    #Message length can be inferred based on msgField. message length is always the same
CUSTOM = object()   #custom means telem and vitals have a hardcoded msg format 
    #In this case, the script doesnt really do anything, telem and vitals code will manually tell it how to increment byteCount

v2t_num_dynamicBits = 4  #Dynamic packets use 4 bits to specify length, so can be size up to 15.
t2v_num_dynamicBits = 4  #Dynamic packets use 8 bits to specify length, so can be size up to 255.
#"telem to vitils, or "vitals to telem"

from dataclasses import dataclass, field

from parseFile import globalEnums, globalDefines

nodeCount = maxFrameCnt = maxDataCnt = 0

@dataclass
class msgField:
    name: str
    bits: int = 0
    min: int = 0
    max: int | None = None  #if not set, will assume unsigned and set max = 2^bits - 1
    isEnum: bool = False #if specified, will look up the enum by name in globalEnums to set bits, min, max

#check if callable(value)

def setPacketParameters(set_nodeCount, set_maxFrameCnt, set_maxDataCnt):
    global nodeCount, maxFrameCnt, maxDataCnt
    nodeCount, maxFrameCnt, maxDataCnt = set_nodeCount, set_maxFrameCnt, set_maxDataCnt

#how many bits will be needed to specify flags for nodes responding to HB
def get_nodeCount():
    assert nodeCount > 0
    return nodeCount   #each node gets 1 bit in HBTimingMask

def get_maxFrameCntBits():
    assert maxFrameCnt > 0
    return (maxFrameCnt - 1).bit_length() 
def get_maxFrameCntBytes():
    return (get_maxFrameCntBits() + 7) // 8
def get_maxDataCntBits():
    assert maxDataCnt > 0
    return (maxDataCnt - 1).bit_length()


# Fixed means script will infer the size based on msgFields declared.
# Custom means the size can vary. The last msgField size in a custom message should be custom (if any is given)
vitals_to_telem = [
    {"name": "HBTiming", "mask_bits": 4, "mask": 0b1000, "byteCount": FIXED,
        "msgFields": [
            msgField(name="slowestNode1_ID", bits=7),
            msgField(name="slowestNode1_time", bits=10),
            msgField(name="slowestNode2_ID", bits=7),
            msgField(name="slowestNode2_time", bits=10),
            msgField(name="slowestNode3_ID", bits=7),
            msgField(name="slowestNode3_time", bits=10),
        ]
     },
    {"name": "HBStatus", "mask_bits": 4, "mask": 0b1001, "byteCount": FIXED,
        "msgFields": [
            msgField(name="HBMask", bits=get_nodeCount)
        ]
    },
    {"name": "BusStatus", "mask_bits": 4, "mask": 0b1010, "byteCount": FIXED,
        "msgFields": [
            msgField(name = "TWAI_State", isEnum = True), 
            msgField(name="TWAI_TX_Err_Cnt", bits=8),
            msgField(name="TWAI_RX_Err_Cnt", bits=8),
            msgField(name="TWAI_Err_Cnt", bits=12),
            msgField(name="failed_TX_Cnt", bits=12),
            msgField(name="RX_Overrun_Cnt", bits=11),
            msgField(name="RX_Missed_Cnt", bits=11),
            msgField(name="RX_Recv_Queue_Cnt", bits=4)
        ]
     },
    {"name": "vitalsErr", "mask_bits": 4, "mask": 0b1011, "byteCount": CUSTOM},
    {"name": "dataWarning", "mask_bits": 4, "mask": 0b1100, "byteCount": FIXED,
        "msgFields": [
            msgField(name="isCritical", bits=1),
            msgField(name="data_too_high", bits=1),
            msgField(name="extrapolationTrigger", isEnum=True),
            msgField(name="nodeID", bits=7),
            msgField(name="frameID", bits=get_maxFrameCntBits),
            msgField(name="dataID", bits=get_maxDataCntBits),
        ]
    },
    {"name": "nodeStatus", "mask_bits": 4, "mask": 0b1101, "byteCount": FIXED,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            msgField(name="statusUpdates", isEnum=True)
        ]
    },
    {"name": "unknownCanPacket", "mask_bits": 4, "mask": 0b1101, "byteCount": CUSTOM},

    {"name" : "CANDataFrame", "mask_bits": 4, "mask": 0b1110, "byteCount": CUSTOM,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            msgField(name="CANFrame", bits=CUSTOM)
        ]
    }
]

#TODO. Add a target field here. For a sensor, this will auto-generate a function handler for that sensor
#When they recv this command.
#Can choose not to specify a target, and do it manually. 
#If target is vitals, vitals will have a function to handle this named command, without forwarding over CAN
#Will want to make it so byte Count can be set based on log2 (maxFrameCnt) bits, etc.
telem_to_vitals = [
    {"name": "forward_packet", "mask": 0b00000000, "mask_bits": 8, "byteCount": CUSTOM},
    {"name": "enable_HV_standard", "mask": 0b00000001, "mask_bits": 8, "byteCount": FIXED},
    {"name": "enable_HV_override", "mask": 0b00000010, "mask_bits": 8, "byteCount": FIXED},
    {"name": "disable_HV", "mask": 0b00000011, "mask_bits": 8, "byteCount": FIXED},
    {"name": "set_telem_update_frequency_divider", "mask": 0b00000100, "mask_bits": 8, "byteCount": FIXED},
    {"name": "make_non_critical", "mask": 0b00000101, "mask_bits": 8, "byteCount": FIXED},
    {"name": "customChangeDataFlag" , "mask": 0b00000110, "mask_bits": 8, "byteCount": FIXED}
]
#custom
#vitals_to_telem followed by

#forward packet will be followed by the packet to forward (follows standard vitals->telem Can packet forwarding)
#recommended: only forward_packets with function code = TelemetryCommand. but if telem wants to inject native messages, it may.
#enable_HV_override enables HV regardless of vitals_critical_states. not recommended for standard use.
# global enum: telem_to_vitals_commands = {
#     forward_packet = 0b00000000,
#     enable_HV_standard      = 0b00000001,
#     enable_HV_override      = 0b00000010,
#     disable_HV     = 0b00000011,
#     set_telem_update_frequency_divider  = 0b00000100,
#     make_non_critical       = 0b00000101,
# }