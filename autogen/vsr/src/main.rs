use std::path::PathBuf;

use crate::{
    c_gen::write_vsr_state_files, construct_vsr::load_all_vsr_substructs,
    proto_gen::write_proto_file,
};
use clap::Parser;

pub mod c_gen;
pub mod construct_vsr;
pub mod proto_gen;
pub mod schema;
use anyhow::Result;

#[derive(Parser, Debug)]
#[command(about = "Generate VSR proto + C artifacts to an output directory.")]
struct Args {
    #[arg(value_name = "OUTPUT_DIR")]
    output_dir: PathBuf,
}

fn main() -> Result<()> {
    let args = Args::parse();
    println!("starting");
    let substructs = load_all_vsr_substructs()?;

    let proto_out = args.output_dir.join("vsr.proto");
    write_proto_file(&substructs, &proto_out)?;
    println!("wrote {}", proto_out.display());

    let state_h_out = args.output_dir.join("vsr_state.h");
    let state_c_out = args.output_dir.join("vsr_state.c");
    write_vsr_state_files(&substructs, &state_h_out, &state_c_out)?;
    println!("wrote {}", state_h_out.display());
    println!("wrote {}", state_c_out.display());
    Ok(())
}
