// build.rs

use std::env;
use std::path::PathBuf;

pub fn build_common(sensor_name: &str) {
    // Allow overriding with an environment variable, but default to the name above.
    let sensor_node = env::var("SENSOR_NODE").unwrap_or_else(|_| sensor_name.to_string());
    
    // The codegen script places generated C++ files in `c_src/<sensor_name>/`
    let sensor_c_src_path = format!("../src/sensors/{}", sensor_node);
    let static_dec_path = format!("{}/staticDec.cpp", sensor_c_src_path);
    let node_config_define_val = format!("../{}/myDefines.hpp", sensor_node);
    
    cc::Build::new()
        .file("../src/pecan/common.cpp")
        .file("../src/pecan/vitalsInit.c")
        .file(&static_dec_path)
        .include("../src") // Include root of C source to find pecan/pecan.h etc.
        .include(&sensor_c_src_path) // Include path for the sensor's myDefines.hpp
        .define("NODE_CONFIG", Some(node_config_define_val.as_str()))
        .flag("-std=c++23")
        .flag("-mcpu=cortex-m4")
        .flag("-mthumb")
        .flag("-mfloat-abi=hard")
        .flag("-ffunction-sections")
        .flag("-fdata-sections")
        .compile("pecan_bindings"); // The library name

    // Re-run build script if wrapper.h or the generated C++ file changes.
    // bindgen's CargoCallbacks will handle re-running for included headers.
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed={}", static_dec_path);

    // 2. Generate Bindings
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        // Define NODE_CONFIG so sensorHelper.hpp can find the node-specific myDefines.hpp
        .clang_arg(format!("-DNODE_CONFIG={}", node_config_define_val))
        .use_core()
        .ctypes_prefix("core::ffi")
        .clang_arg("--target=thumbv7em-none-eabihf")
        .clang_arg("-mcpu=cortex-m4")
        .clang_arg("-mfloat-abi=hard")
        .clang_arg("-mfpu=fpv4-sp-d16")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .derive_default(true)
        .generate()
        .expect("Unable to generate bindings");
    
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
