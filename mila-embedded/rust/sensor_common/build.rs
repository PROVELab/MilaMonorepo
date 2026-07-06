use std::env;
use std::path::PathBuf;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR should be set by cargo"));
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .use_core()
        .ctypes_prefix("core::ffi")
        .clang_arg("-I../../src")
        .allowlist_type("CANPacket")
        .allowlist_type("CANListenParam")
        .allowlist_type("PCANListenParamsCollection")
        .allowlist_type("PCAN_ERR")
        .allowlist_type("pecanInit")
        .allowlist_var("MAX_SIZE_PACKET_DATA")
        .allowlist_var("MAX_PCAN_PARAMS")
        .allowlist_var("MATCH_TYPE_.*")
        .allowlist_function("vitalsInit")
        .allowlist_function("addParam")
        .allowlist_function("combinedID")
        .allowlist_function("exact")
        .allowlist_function("matchID")
        .allowlist_function("matchFunction")
        .allowlist_function("defaultPacketRecv")
        .allowlist_function("sendStatusUpdate")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("unable to generate shared Rust bindings for sensor_common");

    bindings
        .write_to_file(out_path.join("sensor_common_bindings.rs"))
        .expect("unable to write generated shared Rust bindings");

    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed=../../src/pecan/pecan.h");
    println!("cargo:rerun-if-changed=../../src/programConstants.h");
}
