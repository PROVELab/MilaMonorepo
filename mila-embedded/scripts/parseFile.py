import os
import json
from copy import deepcopy
from dataclasses import dataclass
import math
import re

#values must fit in int32_t
INT_MIN = -2147483648
INT_MAX = 2147483647

# Access a field by name. EX: given fields = dataPoint_fieds, name = "min", ACCESS returns the dict for the "min" field
def ACCESS(fields, name):
    return next(field for field in fields if field["name"] == name)

# For expectation field:
# -dontSpecify means the field should not be specified by the user (in the .def file)
# The field is either computed automatically, or used by the program internally.
# -required means this field must be provided by the user (in the .def file)
# -optional means this field will take on the value already in value, unless otherwise specified.

# A copy of each of these will be made for every dataPoint
dataPoint_fields = [                                                    
    {"name": "minCritical",     "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "telemetry"], "isSet": False},
    {"name": "maxCritical",     "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "telemetry"], "isSet": False},
    {"name": "min",             "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "sensor", "telemetry"], "isSet": False},
    {"name": "max",             "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "sensor", "telemetry"], "isSet": False},
    {"name": "minWarning",      "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "telemetry"], "isSet": False},
    {"name": "maxWarning",      "type": "int32_t", "expectation": "optional",    "value": 0,  "node": ["vitals", "telemetry"], "isSet": False},
    {"name": "startingValue",   "type": "int32_t", "expectation": "required",    "value": 0,  "node": ["vitals"], "isSet": False},
    {"name": "crit_count_max",  "type": "uint8_t", "expectation": "optional",    "value": 0,  "node": ["vitals, telemetry"], "isSet": False},  #how many consecutive criticals (after removing outliers) before considered in critical range? default is 1 if unsepecified and a critical range is set. 0 if not critical at all
    {"name": "enum",            "type": "str",     "expectation": "optional",    "value": None, "node": ["vitals", "sensor", "telemetry"], "isSet": False}, # If set, min/max/bits are derived from this enum
    {"name": "crit_count",      "type": "uint8_t", "expectation": "dontSpecify", "value": 0,  "node": ["vitals"], "isSet": False},
    {"name": "bits",            "type": "int8_t",  "expectation": "optional",    "value": 0, "node": ["vitals", "sensor", "telemetry"], "isSet": False},
]

# A copy of each of these will be made for every CANFrame
CANFrame_fields = [  
    {"name": "nodeID",          "type": "int8_t", "expectation": "dontSpecify", "value": 0, "node": ["vitals", "telemetry"], "isSet": False},    # set based on which node this frame belongs to
    {"name": "frameID",         "type": "int8_t", "expectation": "dontSpecify", "value": 0, "node": ["vitals"], "isSet": False},    # explicitly computed by program
    {"name": "numData",         "type": "int8_t", "expectation": "dontSpecify", "value": 0, "node": ["vitals", "sensor", "telemetry"], "isSet": False},     # computed by the program
    {"name": "dataInfo",        "type": "list",   "expectation": "dontSpecify", "value": [], "node": [], "isSet": False},      # List of dataPoint structs; element‐wise parsed. was node: "array", but I think uncessary?
    {"name": "flags",           "type": "int8_t", "expectation": "optional",    "value": 0, "node": ["vitals"], "isSet": False},      # if not specified, set to 0
    {"name": "dataLocation",    "type": "int8_t", "expectation": "optional",    "value": 0, "node": ["vitals"], "isSet": False},      # never needs to be changed
    {"name": "consecutiveMisses","type": "int8_t", "expectation": "optional",   "value": 0, "node": ["vitals"], "isSet": False},
    {"name": "dataTimeout",     "type": "int32_t","expectation": "required",    "value": 0, "node": ["vitals", "telemetry"], "isSet": False},     # Must be specified
    {"name": "frequency",       "type": "int32_t","expectation": "required",    "value": 0, "node": ["sensor"], "isSet": False}
]
#A copy of each of these will be made for every sensor node
vitalsNode_fields = [   # these fields are only used by vitals. ATM each processed manually (dont specify
    {"name": "flags",       "type": "int8_t", "expectation": "dontSpecify", "value": 0, "Atomic" : True, "isSet": False},
    {"name": "milliSeconds","type": "int16_t", "expectation": "dontSpecify", "value": 0, "Atomic" : True, "isSet": False},
    {"name": "numFrames",   "type": "int8_t", "expectation": "dontSpecify", "value": 0, "Atomic" : False, "isSet": False},
    {"name": "CANFrames",   "type": "list",   "expectation": "dontSpecify", "value": [], "Atomic" : False, "isSet": False}  # List of CANFrame structs
]

@dataclass
class EnumEntry:
    name: str
    value_str: str
    value_int: int  # evaluated integer value

@dataclass
class GlobalEnum:
    enum_name: str
    entries: list[EnumEntry]
# Global list to store enums
globalEnums: list[GlobalEnum] = []

@dataclass
class globalDefine:
    name: str
    value_str: str
    value_int: int  # evaluated integer value

# Global list to store global defines (populated during parsing)
globalDefines = []

#this function contains several manual checks to confirm that the values entered for a datapoint make sense
#The function will also set values that were not specified based on other values
#Ex: if minWarning was not specified, it will be set to min, so that it never triggers (comparison is exclusive)
def validate_datapoint(dp, dataName, node_id):
    # Check for required fields first. These should have been set either by the user or by enum logic.
    # This validation is now done after enum processing in parse_config.

    # Retrieve and evaluate values from the datapoint.
    bit_length    = expression_to_int(ACCESS(dp, "bits")["value"])
    min_value     = expression_to_int(ACCESS(dp, "min")["value"])
    max_value     = expression_to_int(ACCESS(dp, "max")["value"])
    minWarningSet= bool(ACCESS(dp, "minWarning")["isSet"])
    minWarning   = expression_to_int(ACCESS(dp, "minWarning")["value"]) if minWarningSet else 0
    maxWarningSet= bool(ACCESS(dp, "maxWarning")["isSet"])
    maxWarning   = expression_to_int(ACCESS(dp, "maxWarning")["value"]) if maxWarningSet else 0
    minCriticalSet= bool(ACCESS(dp, "minCritical")["isSet"])
    minCritical   = expression_to_int(ACCESS(dp, "minCritical")["value"]) if minCriticalSet else 0
    maxCriticalSet= bool(ACCESS(dp, "maxCritical")["isSet"])
    maxCritical   = expression_to_int(ACCESS(dp, "maxCritical")["value"]) if maxCriticalSet else 0
    starting_val  = expression_to_int(ACCESS(dp, "startingValue")["value"])
    crit_count_max_Set= bool(ACCESS(dp, "crit_count_max")["isSet"])
    crit_count_max = expression_to_int(ACCESS(dp, "crit_count_max")["value"]) if crit_count_max_Set else 0

    # Check overall range is valid.
    if not (min_value < max_value):
        print(f"Error: For {dataName} (node {node_id}): \
              Overall range invalid: min ({min_value}) must be less than max ({max_value}).")
        while(1): pass
    # Check that bit_length is appropriate,Compute the number of bits required for the range.
    req = math.log2(max_value - min_value + 1)
    required_bits = math.ceil(req)
    if bit_length != required_bits:
        print(f"Warning: For {dataName} (node {node_id}): Bit length {bit_length} \
              does not match the required {required_bits} for range size ({max_value} - {min_value}).")
        while(1): pass
    # Check that ranges are within in max and min
    if (min_value<INT_MIN):
        print(f"Warning: For {dataName} (node {node_id}): minValue less than INT_MIN.")
        while(1): pass
    if (max_value>INT_MAX):
        print(f"Warning: For {dataName} (node {node_id}): maxValue more than INT_MAX.")
        while(1): pass

    #set ranges so that they will be ignored if they were not set.
    #note: with current system, warning/critical ranges will be triggered on exclusive comparisons.
    if not minWarningSet:
         minWarning = min_value
         ACCESS(dp, "minWarning")["value"]= minWarning
    if not maxWarningSet:
         maxWarning = max_value
         ACCESS(dp, "maxWarning")["value"]= maxWarning
    if not minCriticalSet:
         minCritical = min_value
         ACCESS(dp, "minCritical")["value"]= minCritical
    if not maxCriticalSet:
         maxCritical = max_value
         ACCESS(dp, "maxCritical")["value"]= maxCritical

    #check crit_count_max
    if( minCriticalSet or maxCriticalSet):  #set crit_count_max to 1 by default
        if(not crit_count_max_Set):
            crit_count_max=1
            ACCESS(dp, "crit_count_max")["value"]= crit_count_max
        if crit_count_max == 0:
            print(f"Error: For {dataName} (node {node_id}): crit_count_max shouldn't be zero when critical range is specified.")
            while(1): pass
    else:
        if crit_count_max_Set:
            print(f"Error: For {dataName} (node {node_id}): crit_count_max specified without critical range.")
            while(1): pass
    
    if crit_count_max < 0  or crit_count_max > 255:
        print(f"Error: For critical data: {dataName} (node {node_id}): crit_count_max not in uint8_t range.")
        while(1): pass

    # Check range ordering.
    if(minWarningSet and minWarning<min_value or (maxWarningSet and maxWarning>max_value)):
        print(f"Warning: For {dataName} (node {node_id}): warningRange outside of given range")
        while(1): pass
    if(minCriticalSet and minCritical<min_value or (maxCriticalSet and maxCritical>max_value)):
        print(f"Warning: For {dataName} (node {node_id}): critical outside of given range")
        while(1): pass
    if(minWarningSet and minCriticalSet and minWarning<minCritical \
        or (maxWarningSet and maxCriticalSet and maxWarning>maxCritical)
    ):
        print(f"Warning: For {dataName} (node {node_id}): warningRange outside of critical range")
        while(1): pass


    #check startingVal
    if(starting_val<minWarning or starting_val>maxWarning):
        print(f"Warning: For {dataName} (node {node_id}): startingVal outside of acceptable range")

#update the data in a given set of fields based on what was read from a line
def updateEntries(parsedFields, fields):
    # Handle backward compatibility for bitLength -> bits
    if 'bitLength' in parsedFields:
        parsedFields['bits'] = parsedFields.pop('bitLength')

    for name, value in parsedFields.items():
        found = False
        for field in fields:
            if field["name"] == name:
                found = True
                if field["expectation"] == "dontSpecify":
                    raise ValueError(f"Specified a locked field: '{name}'")
                else:
                    field["value"] = value
                    ACCESS(fields, name)["isSet"] = True
                break # Found it, no need to check other fields
        if not found:
            # Get a list of valid field names for a helpful error message
            valid_names = [f['name'] for f in fields if f['expectation'] != 'dontSpecify']
            raise ValueError(f"No matching field found for '{name}'. Valid fields are: {valid_names}")

import ast
import operator
# Safe evaluator for integer-only expressions and bitwise operators.
def eval_int_expr(expr: str) -> int:
    """
    Accepts strings like:
      "0b11<<3", "4<<1", "0xF>>2", "-(0b101<<2)", "vitalsID", "prechargeID | 1"
    and returns the evaluated integer. It looks up names in globalDefines and globalEnums.
    """
    
    # Build a context for name lookups
    name_context = {}
    for define in globalDefines:
        name_context[define.name] = define.value_int
    for enum in globalEnums:
        for entry in enum.entries:
            # Later definitions can override earlier ones if names conflict.
            name_context[entry.name] = entry.value_int

    node = ast.parse(expr, mode="eval")

    def _eval(n):
        if isinstance(n, ast.Expression):
            return _eval(n.body)

        # Python already parses 0b..., 0o..., 0x..., and decimal ints as ints
        if isinstance(n, ast.Constant) and isinstance(n.value, int):
            return n.value
        
        # Handle name lookups
        if isinstance(n, ast.Name):
            if n.id in name_context:
                return name_context[n.id]
            raise NameError(f"Name '{n.id}' is not defined in global defines or enums.")

        # Handle attribute access like 'enumName.entryName'
        if isinstance(n, ast.Attribute):
            if isinstance(n.value, ast.Name):
                enum_name = n.value.id
                entry_name = n.attr
                for enum_def in globalEnums:
                    if enum_def.enum_name == enum_name:
                        for entry in enum_def.entries:
                            if entry.name == entry_name:
                                return entry.value_int
                raise NameError(f"Enum entry '{enum_name}.{entry_name}' not found.")
            raise TypeError("Unsupported attribute access pattern.")

        # Support unary + and -
        if isinstance(n, ast.UnaryOp) and isinstance(n.op, (ast.UAdd, ast.USub)):
            val = _eval(n.operand)
            return +val if isinstance(n.op, ast.UAdd) else -val

        # Support parentheses via AST structure automatically

        # Support shifts
        if isinstance(n, ast.BinOp) and isinstance(n.op, (ast.LShift, ast.RShift)):
            left = _eval(n.left)
            right = _eval(n.right)
            if not isinstance(left, int) or not isinstance(right, int):
                raise ValueError("Shift operands must be integers.")
            return operator.lshift(left, right) if isinstance(n.op, ast.LShift) else operator.rshift(left, right)

        # Other bitwise ops: |, &, ^
        if isinstance(n, ast.BinOp) and isinstance(n.op, (ast.BitOr, ast.BitAnd, ast.BitXor)):
            left = _eval(n.left); right = _eval(n.right)
            if isinstance(n.op, ast.BitOr):  return left | right
            if isinstance(n.op, ast.BitAnd): return left & right
            return left ^ right

        raise ValueError(f"Unsupported expression component: {ast.dump(n)}")

    return _eval(node)

def expression_to_int(input_str):
    # This function now becomes a simple wrapper around eval_int_expr
    raw = str(input_str).strip()
    try:
        return eval_int_expr(raw)
    except (ValueError, SyntaxError, NameError) as e:
        raise ValueError(f"Could not evaluate expression '{raw}': {e}") from e

# --- parse_config function moved here ---
def parse_config(file_path):
    # Variables storing info as we go about our parsing
    startingNodeID = None # This will be the lowest node ID found
    nodeCount = 0
    frameCount = 0
    maxFrameCount = 0
    maxDataCount = 0


    # Parallel arrays for each node.
    vitalsNodes = []  # stores info for vitals and sensor nodes
    nodeNames = []    # stores the names of each node
    boardTypes = []   # stores the board type of each node. atm "esp" or "arduino"
    dataNames = []    # stores the names of each piece of data (organized per node)
    numData = []      # stores number of datapoints each node has
    node_ids = []     #Array of node ID's

    missingIDs = []   # will be computed based on node_ids
    

    with open(file_path, 'r') as file:
        lines = file.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if line.startswith("#") or not line:
            continue

        # Process node definition
        if line.startswith("node:"):
            node_details = line.split(":")[1].split(",")
            node_info = {k.strip(): v.strip() for k, v in (item.split("=") for item in node_details)}
            node_id = expression_to_int(node_info["id"])
            node_name = node_info["name"]
            board_type= node_info["board"]
            # Initialize vitalsNode
            vitalsNodes.append(deepcopy(vitalsNode_fields)) #create new vitalsNode entry
            nodeNames.append(node_name)
            boardTypes.append(board_type)
            if startingNodeID is None:
                startingNodeID = node_id
            node_ids.append(node_id)
            numData.append(0)
            nodeCount += 1
        # Process a CANFrame
        elif line.startswith("CANFrame"):
            frameCount+=1

            nodeFrames = ACCESS(vitalsNodes[nodeCount - 1], "numFrames")
            nodeFrames["value"] += 1
            numFrames=nodeFrames["value"]

            frame_details = line.split(":")[1].split(",")
            frame_info = {k.strip(): v.strip() for k, v in (item.split("=") for item in frame_details)}

            framesArr = ACCESS(vitalsNodes[nodeCount - 1], "CANFrames")["value"] #list of CANFrames for this node
            framesArr.append(deepcopy(CANFrame_fields)) #add a new CANFrame entry to this list
            frame = framesArr[numFrames - 1]
            ACCESS(frame, "nodeID")["value"] = node_ids[nodeCount - 1]
            ACCESS(frame, "frameID")["value"] = frameCount - 1
            ACCESS(frame, "numData")["value"] = 0

            updateEntries(frame_info, frame)
        # Process a dataPoint
        elif ":" in line:
            # example: temperature: bitLength=7, min=-10, max=117, ...
            frame = ACCESS(vitalsNodes[nodeCount - 1], "CANFrames")["value"][-1]
            dataArr = ACCESS(frame, "dataInfo")["value"]
            dataArr.append(deepcopy(dataPoint_fields))
            dataPoint = dataArr[-1]
            dataNames.append(line.split(":")[0].strip())
            numData[-1] += 1

            data_details = line.split(":")[1].split(",")
            data_info = {k.strip(): v.strip() for k, v in (item.split("=") for item in data_details)}
            updateEntries(data_info, dataPoint)
            
            # If dataPoint is an enum, set its min/max/bits from the enum definition
            enum_name_val = ACCESS(dataPoint, "enum")["value"]
            if enum_name_val:
                if ACCESS(dataPoint, "min")["isSet"] or ACCESS(dataPoint, "max")["isSet"] or ACCESS(dataPoint, "bits")["isSet"]:
                    print(f"Warning: For {dataNames[-1]} (node {node_ids[-1]}): 'min', 'max', or 'bits' should not be set when 'enum' is used. They will be overridden.")
                try:
                    enum_def = next(entry for entry in globalEnums if entry.enum_name == enum_name_val)
                except StopIteration:
                    raise ValueError(f"Enum '{enum_name_val}' specified for dataPoint '{dataNames[-1]}' (node {node_ids[-1]}) not found in enum definitions.")
                
                min_enum_val = min(entry.value_int for entry in enum_def.entries)
                max_enum_val = max(entry.value_int for entry in enum_def.entries)
                bits_needed = max(1, math.ceil(math.log2(max_enum_val + 1)))
                
                ACCESS(dataPoint, "min")["value"], ACCESS(dataPoint, "min")["isSet"] = min_enum_val, True
                ACCESS(dataPoint, "max")["value"], ACCESS(dataPoint, "max")["isSet"] = max_enum_val, True
                ACCESS(dataPoint, "bits")["value"], ACCESS(dataPoint, "bits")["isSet"] = bits_needed, True
                # Also set startingValue if not provided, which satisfies the 'required' expectation.
                if not ACCESS(dataPoint, "startingValue")["isSet"]:
                    ACCESS(dataPoint, "startingValue")["value"] = enum_def.entries[0].value_int
                    ACCESS(dataPoint, "startingValue")["isSet"] = True

            # Now that enums have been processed, check for any remaining required fields.
            for field in dataPoint:
                if field["expectation"] == "required" and not field["isSet"]:
                    raise ValueError(f"For dataPoint '{dataNames[-1]}' (node {node_ids[-1]}): Did not specify required parameter '{field['name']}'")

            validate_datapoint(dataPoint, dataNames[-1], node_ids[-1])
            ACCESS(frame, "numData")["value"] += 1


    # Compute missing IDs
    all_ids = range(min(node_ids), max(node_ids) + 1)
    missingIDs = [node_id for node_id in all_ids if node_id not in node_ids]

    maxFrameCount = max(ACCESS(vitalsNodes[i], "numFrames")["value"] for i in range(nodeCount))
    maxDataCount = max(numData)

    return vitalsNodes, nodeNames, boardTypes, dataNames, numData, \
            node_ids, startingNodeID, missingIDs, nodeCount, frameCount, \
            maxFrameCount, maxDataCount

def _parse_enums_and_defines(file_path):
    """
    Parses a separate file for global enums and defines.
    Populates globalEnums and globalDefines lists.
    """
    with open(file_path, 'r') as file:
        lines = file.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if line.startswith("#") or not line:
            continue

        # Process an enum block
        if line.startswith("global enum"):
            header = line
            block_lines = [header]

            # Only collect more lines if header didn't already close the enum
            if "}" not in header:
                while i < len(lines):
                    raw = lines[i]
                    i += 1
                    s = raw.strip()
                    # skip blank / full-line comments
                    if not s or s.startswith("#"):
                        continue
                    block_lines.append(raw)
                    # stop at the first line that contains a closing brace
                    if "}" in raw:
                        break

            # Parse the collected lines into joined string. remove comments
            joined = "\n".join(l.split("#", 1)[0] for l in block_lines).strip()

            # Parse enum name
            m = re.match(r"global\s+enum:\s*(\w+)\s*=", joined)
            if not m:
                raise ValueError(f"Bad enum declaration header: {joined!r}")
            enum_name = m.group(1)

            # Extract body between braces
            body = joined[joined.find("{") + 1:joined.rfind("}")].strip()

            # Retrieve entries split by commas of the form name=value
            entries = []
            for piece in body.split(","):
                piece = piece.strip()
                if not piece: continue
                if "=" not in piece: raise ValueError(f"Bad enum entry: {piece!r}")
                k, v = [s.strip() for s in piece.split("=", 1)] #split name and value on '='
                entries.append(EnumEntry(k, v, expression_to_int(v)))

            globalEnums.append(GlobalEnum(enum_name, entries))  #add the enum block to the global list

        # Parse global defines. will become #define in C, and final const in Java
        elif line.startswith("global:"):
            # example: global: vitalsID=0b000010, will make: #define vitalsID 2
            split = line.strip().split(":")[1].strip().split("=")
            newGlobal = globalDefine(split[0], split[1], expression_to_int(split[1]))
            globalDefines.append(newGlobal)
