use std::env;
use std::path::PathBuf;

pub fn build_sensor() {
    let out_path = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR should be set by cargo"));
    let c_helper_dir = PathBuf::from("C_Helper");

    // 1. Generate Bindings
    let bindings = bindgen::Builder::default()
        .header(c_helper_dir.join("wrapper.h").to_string_lossy().as_ref())
        .use_core()
        .ctypes_prefix("core::ffi")
        .clang_arg("-I../../src")
        .clang_arg("-I../../src/sensors")
        .clang_arg("-IC_Helper")
        .clang_arg("-I.")
        .clang_arg("-DNODE_CONFIG=C_Helper/myDefines.hpp")
        .clang_arg("-DSENSOR_RUST_BUILD")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("unable to generate Rust bindings for the sensor project");

    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("unable to write generated Rust bindings");

    // 2. BUILD THE PURE C CODE (No .cpp(true))
    cc::Build::new()
        .file("../../src/pecan/common.c")
        .file("../../src/pecan/vitalsInit.c")
        .include(".")
        .include("C_Helper")
        .include("../../src/")
        .include("../../src/sensors/")
        .define("SENSOR_RUST_BUILD", None)
        .define("NODE_CONFIG", Some("C_Helper/myDefines.hpp"))
        .compile("pecan_c"); // This creates libpecan_c.a

    // 3. BUILD THE C++ CODE (With .cpp(true))
    let mut build_cpp = cc::Build::new();
    build_cpp
        .cpp(true)
        .cpp_link_stdlib(None)
        .include(".")
        .include("C_Helper")
        .include("../../src/")
        .include("../../src/sensors/")
        .define("SENSOR_RUST_BUILD", None)
        .define("NODE_CONFIG", Some("C_Helper/myDefines.hpp"))
        .flag("-fno-rtti")
        .flag("-fno-exceptions");

    let generated_sensor_main = std::path::Path::new("src/sensor_main.rs").exists();
    if generated_sensor_main {
        build_cpp.file("../../src/sensors/common/sensorCommon.cpp");
        if std::path::Path::new("C_Helper/sensorRecvLUT.cpp").exists() {
            build_cpp.file("C_Helper/sensorRecvLUT.cpp");
        }
    }

    build_cpp.file("C_Helper/staticDec.cpp");
    build_cpp.compile("c_helpers"); // This creates libc_helpers.a

    // 4. Re-run Triggers
    println!("cargo:rerun-if-changed=C_Helper/wrapper.h");
    println!("cargo:rerun-if-changed=../../src/pecan/common.c");
    println!("cargo:rerun-if-changed=../../src/sensors/common/sensorCommon.cpp");
    println!("cargo:rerun-if-changed=C_Helper/staticDec.cpp");
    println!("cargo:rerun-if-changed=C_Helper/myDefines.hpp");
    println!("cargo:rerun-if-changed=C_Helper/sensorRecvLUT.cpp");
}
