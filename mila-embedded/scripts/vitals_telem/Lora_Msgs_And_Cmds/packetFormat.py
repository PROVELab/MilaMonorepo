FIXED = object()    #Message length can be inferred based on msgField. message length is always the same
CUSTOM = object()   #custom means telem and vitals have a hardcoded msg format 
    #In this case, the script doesnt really do anything, telem and vitals code will manually tell it how to increment byteCount
PACK_MINIMUM_BITS = object() #script will set mask_bits to highest number it can without adding extra bytes to the msg
PACK_MINIMUM_BITS_PLUS_8 = object() #add 8 to minimum bits, prevent overflowing masks for uncommon messages

v2t_num_dynamicBits = 4  #Dynamic packets use 4 bits to specify length, so can be size up to 15.
t2v_num_dynamicBits = 4  #Dynamic packets use 8 bits to specify length, so can be size up to 255.
#"telem to vitils, or "vitals to telem"

from dataclasses import dataclass, field
from os import name

from config.parseFile import globalEnums, globalDefines

nodes_list = []
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

def setPacketParameters(set_nodes, set_maxFrameCnt, set_maxDataCnt):
    global nodes_list, nodeCount, maxFrameCnt, maxDataCnt
    nodes_list = set_nodes
    nodeCount = len(nodes_list)
    maxFrameCnt, maxDataCnt = set_maxFrameCnt, set_maxDataCnt



def get_maxnodeValueBits():
    if not nodes_list:
        return 7 # Fallback
    
    max_node_id = max(node.id for node in nodes_list) if nodes_list else 0
    
    # Also consider special IDs from enums
    special_ids_enum = next((e for e in globalEnums if e.enum_name == "specialIDs"), None)
    if special_ids_enum:
        max_special_id = max(entry.value_int for entry in special_ids_enum.entries)
        max_node_id = max(max_node_id, max_special_id)

    # .bit_length() is 0 for 0, but we need at least 1 bit if ID 0 exists.
    return max(1, max_node_id.bit_length())

def get_maxFrameCntBits():
    assert maxFrameCnt > 0
    return (maxFrameCnt - 1).bit_length() 
def get_maxFrameCntBytes():
    return (get_maxFrameCntBits() + 7) // 8
def get_maxDataCntBits():
    assert maxDataCnt > 0
    return (maxDataCnt - 1).bit_length()

def get_nodeCount():
    return nodeCount


# Fixed means script will infer the size based on msgFields declared.
# Custom means the size can vary. a "payload" field is inferred at the end of any byteCount: CUSTOM msg.
vitals_to_telem = [
    {"name": "HBTiming", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 3000, #expected every second
        "msgFields": [
            msgField(name="slowestNode1_ID", bits=get_maxnodeValueBits, plottable=True, maxWarning=100), #should be <100ms to respond to HB
            msgField(name="slowestNode1_time", bits=10, plottable=True),
            msgField(name="slowestNode2_ID", bits=get_maxnodeValueBits, plottable=True, maxWarning=100),
            msgField(name="slowestNode2_time", bits=10, plottable=True),
            msgField(name="slowestNode3_ID", bits=get_maxnodeValueBits, plottable=True, maxWarning=100),
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
    {"name": "dataWarning", "mask_bits": PACK_MINIMUM_BITS_PLUS_8, "byteCount": FIXED, "dataTimeout": 0,
        "msgFields": [
            msgField(name="dataTooHigh", bits=1),
            msgField(name="dataErrorTrigger", enum=True),
            msgField(name="nodeID", bits=get_maxnodeValueBits),
            msgField(name="frameID", bits=get_maxFrameCntBits),
            msgField(name="dataID", bits=get_maxDataCntBits),
        ]
    },
    {"name": "frameWarning", "mask_bits": PACK_MINIMUM_BITS_PLUS_8, "byteCount": FIXED, "dataTimeout": 0,
        "msgFields": [
            msgField(name="frameErrorTrigger", enum=True),
            msgField(name="nodeID", bits=get_maxnodeValueBits),
            msgField(name="frameID", bits=get_maxFrameCntBits),
        ]
    },
    {"name": "nodeStatus", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, "dataTimeout": 0,
        "msgFields": [
            msgField(name="nodeID", bits=get_maxnodeValueBits),
            msgField(name="statusUpdates", enum=True)
        ]
    },
    {"name": "unknownCanPacket", "mask_bits": PACK_MINIMUM_BITS, "byteCount": CUSTOM, "dataTimeout": 0,
        "msgFields": [
            msgField(name="nodeID", bits=11), #the ID of the node that sent this packet
            msgField(name="DLC", bits=4), #the length of the data in bytes
            msgField(name="extendedIDPresent", bits=1), #if extendedID = 1
            msgField(name="RTR", bits=1), #if RTR = 1, then this is a request packet with no data, and the dataLength field is actually the length of the requested data.
            msgField(name="ext_id_start", bits=2),  #2 + 16 = the full 18 bits of extId
            #payload of 2 bytes extID if extendedID = 1, 
            #payload of length DLC if RTR != 0.
        ]},

    {"name" : "CANDataFrame", "mask_bits": PACK_MINIMUM_BITS,  "byteCount": CUSTOM, "dataTimeout": 0,
        "msgFields": [
            msgField(name="nodeID", bits=get_maxnodeValueBits),
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
    {"name": "set_telem_update_frequency", "targetNode": "vitals", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED,
        "msgFields": [
            msgField(name="nodeID", bits=get_maxnodeValueBits), # Target node, or vitalsID for vitals packets
            # If nodeID is a sensor, this is frameID. If nodeID is vitalsID, this is the packet_idx of a vitals_to_telem packet.
            msgField(name="packet_or_frame_ID", bits=8),
            msgField(name="divider", bits=8) #the divider for telemetry update frequency.
    ]},

    {"name": "prechargeCommand", "targetNode": "prechargeID", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED, 
        "msgFields": [
            msgField(name="prechargeCommands", enum=True)
        ]
    },
    {"name": "intermoduleCommand", "targetNode": "powerDistribution", "mask_bits": PACK_MINIMUM_BITS, "byteCount": FIXED,
        "msgFields": [
            msgField(name="prechargeCommands", enum=True)
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