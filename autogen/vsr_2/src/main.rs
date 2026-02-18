use std::path::Path;

use crate::{
    c_gen::write_vsr_state_files, construct_vsr::load_all_vsr_substructs,
    proto_gen::write_proto_file,
};

pub mod c_gen;
pub mod construct_vsr;
pub mod proto_gen;
pub mod schema;
use anyhow::Result;

fn main() -> Result<()> {
    println!("starting");
    let substructs = load_all_vsr_substructs()?;

    let proto_out = Path::new(env!("CARGO_MANIFEST_DIR")).join("generated/vsr.proto");
    write_proto_file(&substructs, &proto_out)?;
    println!("wrote {}", proto_out.display());

    let state_h_out = Path::new(env!("CARGO_MANIFEST_DIR")).join("generated/vsr_state.h");
    let state_c_out = Path::new(env!("CARGO_MANIFEST_DIR")).join("generated/vsr_state.c");
    write_vsr_state_files(&substructs, &state_h_out, &state_c_out)?;
    println!("wrote {}", state_h_out.display());
    println!("wrote {}", state_c_out.display());
    Ok(())
}
