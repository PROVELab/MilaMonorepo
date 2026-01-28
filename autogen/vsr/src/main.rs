use crate::{vsr::VSR_METADATA_IMPL, vsr_codegen::generate_c};
use std::fs;
use std::path::PathBuf;

// Clap for argument parsing
use clap::Parser;

pub mod vsr_macro;
pub mod vsr_metadata;
pub mod vsr_codegen;
pub mod vsr;

/// CLI for generating VSR C files.
/// If no output directory is provided, the directory of the Cargo.toml
/// (i.e. the crate root) is used.
#[derive(Parser, Debug)]
#[command(
    name = "vsr-gen",
    about = "Generate VSR C header and source files",
    version
)]
struct Cli {
    /// Output directory for the generated `vsr.h` and `vsr.c` files.
    /// Defaults to the crate root (the directory containing Cargo.toml).
    #[arg(
        value_name = "OUT_DIR",
        default_value = env!("CARGO_MANIFEST_DIR")
    )]
    out_dir: PathBuf,
}

fn main() {
    // Parse CLI arguments (out_dir will always be set, either by user or default)
    let cli = Cli::parse();

    // Generate the C code
    let (h, c) = generate_c(&VSR_METADATA_IMPL);

    // Build full paths inside the chosen directory
    let mut header_path = cli.out_dir.clone();
    header_path.push("vsr.h");

    let mut source_path = cli.out_dir.clone();
    source_path.push("vsr.c");

    // Write the files, propagating any I/O errors
    fs::write(&header_path, h).expect("Unable to write header file");
    fs::write(&source_path, c).expect("Unable to write source file");

    println!("Successfully generated VSR code:");
    println!("  Header: {}", header_path.display());
    println!("  Source: {}", source_path.display());
}
