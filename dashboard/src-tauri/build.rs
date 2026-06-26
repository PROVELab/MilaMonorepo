use std::{env, path::PathBuf};

use vsr::{
    construct_vsr::{load_all_vsr_substructs, vsr_definition_dirs},
    proto_gen::write_proto_file,
};

fn main() {
    tauri_build::build();

    for dir in vsr_definition_dirs().expect("failed to locate VSR definition dirs") {
        println!("cargo:rerun-if-changed={}", dir.display());
    }
    println!("cargo:rerun-if-changed=../../autogen/vsr/templates/vsr.proto.j2");

    let substructs = load_all_vsr_substructs().expect("failed to load VSR defs");
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR missing"));
    let proto_out = out_dir.join("vsr.proto");
    let descriptor_out = out_dir.join("vsr_descriptor.bin");
    write_proto_file(&substructs, &proto_out).expect("failed to write generated vsr.proto");

    let mut prost_config = prost_build::Config::new();
    prost_config.file_descriptor_set_path(&descriptor_out);
    prost_config
        .compile_protos(&[proto_out], &[out_dir])
        .expect("failed to compile generated VSR protobuf");
}
