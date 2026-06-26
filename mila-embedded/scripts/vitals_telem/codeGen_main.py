import os
from config.parseFile import parse_config
from genSensors import createSensors 
from genVitals import createVitals
from genTelemetry import createTelemetry
from constantGen import writeConstants
from Lora_Msgs_And_Cmds.packetFormat import vitals_to_telem, telem_to_vitals

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))

    node_def_file_name = "simpleTest.def"
    enum_def_file_name = "enum.def"
    node_def_file_path = os.path.join(script_dir, "config", "simpleTest.def")
    enum_def_file_path = os.path.join(script_dir, "config", "enum.def")

    gen_dir = os.path.join(script_dir, f"generated_{os.path.splitext(os.path.basename(node_def_file_name))[0]}")
    generated_platformio_path = os.path.join(gen_dir, "Generatedplatformio.ini")

    src_dir = os.path.normpath(os.path.join(script_dir, "..", "..", "src"))
    src_program_constants_path = os.path.join(src_dir, "programConstants.h")
    src_sensors_dir = os.path.join(src_dir, "sensors")

    vitals_dir = os.path.join(src_dir, "vitalsNode")
    vitals_gen_dir = os.path.join(vitals_dir, "vitalsGen")
    vitals_callbacks_dir = os.path.join(vitals_dir, "callbacks")

    telem_main_dir = os.path.normpath(
        os.path.join(script_dir, "..", "..", "..", "telem-dashboard", "src", "main")
    )
    telem_dir = os.path.join(telem_main_dir, "java")
    telem_resources_dir = os.path.join(telem_main_dir, "resources")

    telem_util_dir = os.path.join(telem_dir, "util")
    telem_lookup_dir = os.path.join(telem_dir, "lookup")
    telem_presentation_dir = os.path.join(telem_dir, "presentation")

    telem_csv_path = os.path.join(telem_resources_dir, "telemetry.csv")
    telem_util_constants_path = os.path.join(telem_util_dir, "Constants.java")
    telem_records_path = os.path.join(telem_lookup_dir, "TelemetryRecords.java")

    rust_root_dir = os.path.normpath(os.path.join(script_dir, "..", "..", "rust"))
    rust_constants_path = os.path.join(rust_root_dir, "programConstants.rs")

    os.makedirs(gen_dir, exist_ok=True)
    os.makedirs(src_sensors_dir, exist_ok=True)
    os.makedirs(vitals_gen_dir, exist_ok=True)
    os.makedirs(vitals_callbacks_dir, exist_ok=True)
    os.makedirs(telem_util_dir, exist_ok=True)
    os.makedirs(telem_lookup_dir, exist_ok=True)
    os.makedirs(telem_presentation_dir, exist_ok=True)
    os.makedirs(rust_root_dir, exist_ok=True)

    #list of nodes + meta-data, asw as other fields for CANFrames.
    (nodes, fields) = parse_config(node_def_file_path, enum_def_file_path, gen_dir)

    writeConstants("c", src_program_constants_path, nodes, fields, len(vitals_to_telem))
    writeConstants("java", telem_util_constants_path, nodes, fields, len(vitals_to_telem))
    writeConstants("rust", rust_constants_path, nodes, fields, len(vitals_to_telem))
    
    createSensors(nodes, script_dir, telem_to_vitals, src_sensors_dir, rust_root_dir, generated_platformio_path)
    createVitals(nodes, fields, vitals_to_telem, telem_to_vitals, vitals_gen_dir, vitals_callbacks_dir)
    
    createTelemetry(nodes, fields, vitals_to_telem, telem_to_vitals, # <- parameters
                    telem_presentation_dir, telem_csv_path, telem_records_path # <- paths
    )

    print("Done.")
