// build.rs

use std::env;

fn main() {
    // Read the sensor target from the environment (default to pedalSensor if not set)
    let sensor_node = env::var("SENSOR_NODE").unwrap_or_else(|_| "pedalSensor".to_string());
    
    // Pass it as a rustc cfg so you can do #[cfg(sensor = "pedalSensor")] in Rust if needed
    println!("cargo:rustc-cfg=sensor=\"{}\"", sensor_node);
    
    let static_dec_path = format!("generated_rust/{}/sensorStaticDec.cpp", sensor_node);
    
    cc::Build::new()
        .file("../src/pecan/common.cpp")
        .file(static_dec_path)
        .include("c_src")
        .flag("-std=c++23")
        .flag("-mcpu=cortex-m4")
        .flag("-mthumb")
        .flag("-mfloat-abi=hard")
        .flag("-ffunction-sections")
        .flag("-fdata-sections")
        .compile("pecan_common"); // The library name

    // Re-run if either header changes
    println!("cargo:rerun-if-changed=../src/pecan/pecan.h");
    println!("cargo:rerun-if-changed=../src/programConstants.h");
    println!("cargo:rerun-if-changed=wrapper.h");

    // 2. Generate Bindings
    let bindings = bindgen::Builder::default()
        // Point to the wrapper that includes BOTH files
        .header("wrapper.h")
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

    let out_path = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
