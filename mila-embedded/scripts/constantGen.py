

import os
from parseFile import dataPoint_fields, CANFrame_fields, vitalsNode_fields, globalDefines, globalEnums, ACCESS


 # === Precompute node IDs in Python, for Java Constants
def genNodeIDsJava(f, missingIDs, numberOfNodes, startingOffset):

    nodeIDs = []
    mi = 0
    current = startingOffset
    while len(nodeIDs) < numberOfNodes:
        if mi < len(missingIDs) and current == missingIDs[mi]:
            mi += 1
            current += 1
            continue
        nodeIDs.append(current)
        current += 1

    node_elems = ", ".join(str(x) for x in nodeIDs)
    f.write("\n\tpublic static final int[] nodeIDs = new int[]{ " + node_elems + " };\n")

#write enums to file f, in either C or Java syntax    
def writeEnums(f, lang_l: str):
    if not globalEnums:
        return

    if lang_l == "c":
        for g in globalEnums:
            f.write(
                f'\n// global enum {g.enum_name}\n'
                f'typedef enum {{\n'
            )
            for i, e in enumerate(g.entries):
                comma = ',' if i < len(g.entries) - 1 else ''
                f.write(f'\t{e.name} = {e.value_int}{comma}\t/* {e.value_str} */\n')
            f.write(
                f'}} {g.enum_name};\n'
            )

    elif lang_l == "java":
        if not globalEnums:
            return

        for g in globalEnums:
            # Class header
            f.write(
                f'\n\t// global enum {g.enum_name}\n'
                f'\tpublic static final class {g.enum_name} {{\n'
                f'\t\tprivate {g.enum_name}() {{}}\n'
            )
            # Constants
            for e in g.entries:
                f.write(f'\t\tpublic static final int {e.name} = {int(e.value_int)};\t// {e.value_str}\n')
            # Close class
            f.write('\t}\n')


# Writes constants files for either C or java depending on lang
def writeConstants(lang, constants_file_path, minId, numMissingIDs, nodeCount, frameCount, globalDefines, missingIDs):
    lang_l = lang.lower()

    # Choose output path and wrappers
    if lang_l == "java":
        out_path = os.path.join(os.path.dirname(constants_file_path), "Constants.java")
        pre_wrapper_open  = "public final class Constants {\n    private Constants() {}\n\n"
        pre_wrapper_close = "}\n"
        # The ONLY difference in the main body: how each constant is emitted
        const_decl = "public static final int {name} = {value};"
        indent = "    "  # indent inside class
    elif lang_l == "c":
        out_path = constants_file_path
        pre_wrapper_open  = "#ifndef progConsts\n#define progConsts\n\n"
        pre_wrapper_close = "\n#endif\n"
        const_decl = "#define {name} {value}"
        indent = ""       # no extra indent in C
    else:
        raise ValueError(f"Unsupported lang: {lang!r}")

    with open(out_path, "w") as f:
        # Write wrapper open
        f.write(pre_wrapper_open)

        # ===== Main body (shared) =====
        f.write(f"{indent}//generated Constants\n")
        f.write(f"{indent}{const_decl.format(name='numberOfNodes', value=nodeCount)}\n")
        f.write(f"{indent}{const_decl.format(name='totalNumFrames', value=frameCount)}\n")
        f.write(f"{indent}{const_decl.format(name='numMissingIDs', value=numMissingIDs)}\n")
        f.write(f"{indent}{const_decl.format(name='startingOffset', value=minId)}\n")
        f.write(f"\n{indent}//Explicilty defined in sensors.def constants\n")

        # Shared loop & eval logic — identical for both languages
        for define in globalDefines:
            # Single place that switches syntax via const_decl
            f.write(f"{indent}{const_decl.format(name=define.name, value=int(define.value_int))}\t\t// {define.value_str}\n")
        writeEnums(f, lang)
         # === Add missingIDs as a static final array (Java only) ===
        if lang_l == "java" and missingIDs:
            genNodeIDsJava(f, missingIDs, nodeCount, minId)

        f.write(pre_wrapper_close)
        f.close()