FIXED = object()    #Message length can be inferred based on msgField. message length is always the same
CUSTOM = object()   #custom means telem and vitals have a hardcoded msg format 
    #In this case, the script doesnt really do anything, telem and vitals code will manually tell it how to increment byteCount
PACK_MINIMUM_BITS = object() #script will set mask_bits to highest number it can without adding extra bytes to the msg

v2t_num_dynamicBits = 4  #Dynamic packets use 4 bits to specify length, so can be size up to 15.
t2v_num_dynamicBits = 4  #Dynamic packets use 8 bits to specify length, so can be size up to 255.
#"telem to vitils, or "vitals to telem"

from dataclasses import dataclass, field
from os import name

from parseFile import globalEnums, globalDefines

nodeCount = maxFrameCnt = maxDataCnt = 0

@dataclass
class msgField:
    name: str
    bits: int = 0
    min: int = 0
    max: int | None = None  #if not set, will assume unsigned and set max = 2^bits - 1
    enum: str | bool | None = None # If str, use as enum name. If True, use field.name.
    plottable: bool = False
    # Telemetry properties for plottable fields, to be written to telemetry.csv
    minWarning: int | None = None
    maxWarning: int | None = None
    minCritical: int | None = None
    maxCritical: int | None = None
    crit_count_max: int | None = None
    startingValue: int | None = None
    flags: int | None = None


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
    {"name": "HBTiming", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 3000, #expected every second
        "msgFields": [
            msgField(name="slowestNode1_ID", bits=7, plottable=True, maxWarning=100), #should be <100ms to respond to HB
            msgField(name="slowestNode1_time", bits=10, plottable=True),
            msgField(name="slowestNode2_ID", bits=7, plottable=True, maxWarning=100),
            msgField(name="slowestNode2_time", bits=10, plottable=True),
            msgField(name="slowestNode3_ID", bits=7, plottable=True, maxWarning=100),
            msgField(name="slowestNode3_time", bits=10, plottable=True),
        ]
     },
    {"name": "HBStatus", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 3000, #expected every second
        "msgFields": [
            msgField(name="HBMask", bits=get_nodeCount)
        ]
    },
    {"name": "BusStatus", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 3000, #expected every second
        "msgFields": [
            msgField(name = "TWAI_STATE", enum = True),     #TODO: add reaspmable warning values here
            msgField(name="TWAI_TX_Err_Cnt", bits=8, min=-10, max=245, plottable=True),
            msgField(name="TWAI_RX_Err_Cnt", bits=8, plottable=True),
            msgField(name="TWAI_Err_Cnt", bits=12, plottable=True),
            msgField(name="failed_TX_Cnt", bits=12, plottable=True),
            msgField(name="RX_Overrun_Cnt", bits=11, plottable=True),
            msgField(name="RX_Missed_Cnt", bits=11, plottable=True),
            msgField(name="RX_Recv_Queue_Cnt", bits=4, plottable=True)
        ]
     },
    {"name": "vitalsErr", "mask_bits": PACK_MINIMUM_BITS, "byteCount": CUSTOM, "dataTimeout": 0, #Lora errors, can be variable length, as are 2 byte errors with 1 byte count of how mnany
        "msgFields": [
            msgField(name="numErrors", bits=8)
            #followed by numErrors repetitions of:
            #   msgField(name="errorCode", bits=16)
        ]
    },
    {"name": "dataWarning", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 0,
        "msgFields": [
            msgField(name="data_too_high", bits=1),
            msgField(name="extrapolationDueToTimeout", bits=1),
            msgField(name="errorTrigger", enum=True),
            msgField(name="nodeID", bits=7),
            msgField(name="frameID", bits=get_maxFrameCntBits),
            msgField(name="dataID", bits=get_maxDataCntBits),
        ]
    },
    {"name": "nodeStatus", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 0,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            msgField(name="statusUpdates", enum=True)
        ]
    },
    {"name": "unknownCanPacket", "mask_bits": PACK_MINIMUM_BITS, "byteCount": CUSTOM, "dataTimeout": 0},
        #payload contains standard CAN packet packing (a bit TBD atm)

    {"name" : "CANDataFrame", "mask_bits": PACK_MINIMUM_BITS,  "byteCount": CUSTOM, "dataTimeout": 0,
        "msgFields": [
            msgField(name="nodeID", bits=7),
            #payload contains frameID, followed by packed data.
        ]
    }
]

#only change from vitals_to_telem is the targetNode field (required)
#if targetNode is vitals, vitals handles internally. 
#if targetNode is not vitals, vitals will forward the packet to the target node,
# sending the CAN msg with 7-bit id of the target node, and 4-bit function code for telemetry command
# All nodes will have a pecan handler for msgs with their own id + telemetry command.

telem_to_vitals = [
    {"name": "genericVitalsCommand", "targetNode": "vitals", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED,
        "msgFields": [
            msgField(name="vitalsCommands", enum=True)
        ]},
    {"name": "set_telem_update_frequency", "targetNode": "vitals", "name": "set_telem_update_frequency_divider", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED,
        "msgFields": [
            msgField(name="divider", bits=4) #the divider for telemetry update frequency.
    ]},

    {"name": "prechargeCommand", "targetNode": "prechargeID", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, 
        "msgFields": [
            msgField(name="prechargeCommands", enum=True)
        ]
    },
    {"name": "prechargeValue", "targetNode": "prechargeID", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, 
        "msgFields": [
            msgField(name="value1", bits=16)
        ]
    },

    {"name": "forward_packet", "targetNode": "vitals", "mask_bits": PACK_MINIMUM_BITS, "byteCount": CUSTOM, 
        "msgFields": [ 
            msgField(name="CAN_ID", bits=11), #the ID of the node to forward this packet to. only used if RTR = 1
            msgField(name="dataLength", bits=4), #the length of the data in bytes. only used if RTR = 0
            msgField(name="extendedID", bits=1), #if extendedID = 1, then this is an extended CAN packet. if extendedID = 0, then this is a standard CAN packet.
            #payload =
                    #1 to 8 bytes of data based on dataLength
                    #3 bytes extended Can ID (if extendedID = 1), only actually 18 bits used (18 bit unsigned int)

        ]
     },
]