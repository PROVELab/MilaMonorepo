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
# Custom means the size can vary. a "payload" field is inferred at the end of any byteCount: CUSTOM msg.
vitals_to_telem = [
    {"name": "HBTiming", "mask_bits": 4, "byteCount": FIXED,
        "msgFields": [
            msgField(name="slowestNode1_ID", bits=7),
            msgField(name="slowestNode1_time", bits=10),
            msgField(name="slowestNode2_ID", bits=7),
            msgField(name="slowestNode2_time", bits=10),
            msgField(name="slowestNode3_ID", bits=7),
            msgField(name="slowestNode3_time", bits=10),
        ]
     },
    {"name": "HBStatus", "mask_bits": 4, "byteCount": FIXED,
        "msgFields": [
            msgField(name="HBMask", bits=get_nodeCount)
        ]
    },
    {"name": "BusStatus", "mask_bits": 4, "byteCount": FIXED,
        "msgFields": [
            msgField(name = "TWAI_STATE", isEnum = True), 
            msgField(name="TWAI_TX_Err_Cnt", bits=8),
            msgField(name="TWAI_RX_Err_Cnt", bits=8),
            msgField(name="TWAI_Err_Cnt", bits=12),
            msgField(name="failed_TX_Cnt", bits=12),
            msgField(name="RX_Overrun_Cnt", bits=11),
            msgField(name="RX_Missed_Cnt", bits=11),
            msgField(name="RX_Recv_Queue_Cnt", bits=4)
        ]
     },
    {"name": "vitalsErr", "mask_bits": 4, "byteCount": CUSTOM, #Lora errors, can be variable length, as are 2 byte errors with 1 byte count of how mnany
        "msgFields": [
            msgField(name="numErrors", bits=8)
            #followed by numErrors repetitions of:
            #   msgField(name="errorCode", bits=16)
        ]
    },
    {"name": "dataWarning", "mask_bits": 4, "byteCount": FIXED,
        "msgFields": [
            msgField(name="isCritical", bits=1),
            msgField(name="data_too_high", bits=1),
            msgField(name="extrapolationTrigger", isEnum=True),
            msgField(name="nodeID", bits=7),
            msgField(name="frameID", bits=get_maxFrameCntBits),
            msgField(name="dataID", bits=get_maxDataCntBits),
        ]
    },
    {"name": "nodeStatus", "mask_bits": 4, "byteCount": FIXED,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            msgField(name="statusUpdates", isEnum=True)
        ]
    },
    {"name": "unknownCanPacket", "mask_bits": 4, "byteCount": CUSTOM},
        #payload contains standard CAN packet packing (a bit TBD atm)

    {"name" : "CANDataFrame", "mask_bits": 4,  "byteCount": CUSTOM,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            #payload contains frameID, followed by packed data.
        ]
    }
]

# can define these for each node
# One byte at the start of each telem to vitals packet will correspond to a each value in each command
# vitals when then match the value here to a node based on what range of values tha value falls in.
# For target = vitals, it will just call a handler functionbased on the command
# Otherwise, vitals will forward the packet as an RTR message (as it is a command), with ID = the target node's ID,
#Any node listed here will have a file of handlers automatically generated. The function will be passed the data

# TODO:: We should update matcher in pecan to require the correct value of RTR be set to match

#make forward packet a vitals command. everything from telem_to_vitals is a command
#how handle longer data? use table

#Should just use the same format as vitals_to_telem, just have a target node specified as well.

#prefer this, just want . shut up.
nodeCommands = [
    {"targetNode": "vitals",    "enum_name": "vitalsCommands"},
    {"targetNode": "precharge", "enum_name": "prechargeCommands"}
]


telem_to_vitals = [
    #List of enums. this will reserve masks, and just be sent as a byte
    {"targetNode:": "vitals", "enum_name": "vitalsCommands"},
    {"targetNode:": "precharge", "enum_name": "prechargeCommands"},

    {"name": "forward_packet", "mask_bits": 8, "byteCount": CUSTOM, "targetNode": "vitals"},
    {"name": "set_telem_update_frequency_divider", "mask_bits": 8, "byteCount": FIXED, "targetNode": "vitals"},
    {"name": "make_non_critical", "mask_bits": 8, "byteCount": FIXED, "targetNode": "vitals"},
    {"name": "customChangeDataFlag" , "mask": 0b00000110, "mask_bits": 8, "byteCount": FIXED, "targetNode": "airPressureSensorEsp"}
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