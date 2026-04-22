use std::{env, path::PathBuf};

use vsr::{construct_vsr::load_all_vsr_substructs, proto_gen::write_proto_file};

fn main() {
    tauri_build::build();

    println!("cargo:rerun-if-changed=../../autogen/vsr/defs");
    println!("cargo:rerun-if-changed=../../autogen/vsr/templates/vsr.proto.j2");

    let substructs = load_all_vsr_substructs().expect("failed to load VSR defs");
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR missing"));
    let proto_out = out_dir.join("vsr.proto");
    write_proto_file(&substructs, &proto_out).expect("failed to write generated vsr.proto");

    prost_build::Config::new()
        .compile_protos(&[proto_out], &[out_dir])
        .expect("failed to compile generated VSR protobuf");
}
