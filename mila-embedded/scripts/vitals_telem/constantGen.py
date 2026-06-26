from typing import Any, TextIO, List

from config.parseFile import ParsedFields, Node


 # === Precompute node IDs in Python, for Java Constants
def genNodeIDsJava(
    f: TextIO,
    nodes: list[Node]
) -> None:
    nodeIDs = [node.node_id for node in nodes]
    node_elems = ", ".join(str(x) for x in nodeIDs)
    f.write("\n\tpublic static final int[] nodeIDs = new int[]{ " + node_elems + " };\n")

#write enums to file f, in either C or Java syntax    
def writeEnums(f: TextIO, lang_l: str, global_enums: list[Any]) -> None:
    if not global_enums:
        return

    if lang_l == "c":
        for g in global_enums:
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
        if not global_enums:
            return

        for g in global_enums:
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

    elif lang_l == "rust":
        for g in global_enums:
            f.write(
                f"\n// global enum {g.enum_name}\n"
                f"pub mod {g.enum_name} {{\n"
            )
            for e in g.entries:
                f.write(f"    pub const {e.name}: u32 = {int(e.value_int)};\t// {e.value_str}\n")
            f.write("}\n")
            f.write(f"pub use {g.enum_name}::*;\n")


# Writes constants files for either C, java, or rust depending on lang
def writeConstants(lang: str,
                   constants_file_path: str,
                   nodes: List[Node],
                   fields: ParsedFields, #uses globalDefines and globalEnums (the constants)
                   num_vitals_to_telem_packets: int = 0) -> None:
    lang_l = lang.lower()

    # Choose output path and wrappers
    if lang_l == "java":
        out_path = constants_file_path
        pre_wrapper_open  = "package util;\npublic final class Constants {\n    private Constants() {}\n\n"
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
    elif lang_l == "rust":
        out_path = constants_file_path
        pre_wrapper_open = "#![allow(dead_code)]\n#![allow(non_snake_case)]\n#![allow(non_upper_case_globals)]\n#![allow(unused_imports)]\n\n"
        pre_wrapper_close = ""
        const_decl = "pub const {name}: u32 = {value};"
        indent = ""
    else:
        raise ValueError(f"Unsupported lang: {lang!r}")

    with open(out_path, "w") as f:
        # Write wrapper open
        f.write(pre_wrapper_open)

        # ===== Main body (shared) =====
        f.write(f"{indent}//generated Constants\n")
        f.write(f"{indent}{const_decl.format(name='numberOfNodes', value=len(nodes))}\n")
        f.write(f"{indent}{const_decl.format(name='totalNumFrames', value=fields.frameCount)}\n")
        if num_vitals_to_telem_packets > 0:
            f.write(f"{indent}{const_decl.format(name='numVitalsToTelemPackets', value=num_vitals_to_telem_packets)}\n")
        f.write(f"\n{indent}//Explicilty defined in sensors.def constants\n")

        # Shared loop & eval logic — identical for both languages
        for define in fields.globalDefines:
            # Single place that switches syntax via const_decl
            f.write(f"{indent}{const_decl.format(name=define.name, value=int(define.value_int))}\t\t// {define.value_str}\n")
        writeEnums(f, lang, fields.globalEnums)
        # Java also gets the explicit list of configured node IDs.
        if lang_l == "java":
            genNodeIDsJava(f, nodes)

        f.write(pre_wrapper_close)
        f.close()
