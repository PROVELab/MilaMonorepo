use crate::{vsr::VSR_METADATA_IMPL, vsr_codegen::generate_c};
use std::fs;

pub mod vsr_macro;
pub mod vsr_metadata;
pub mod vsr_codegen;
pub mod vsr;

fn main() {
    let (h, c) = generate_c(&VSR_METADATA_IMPL);
    
    let header_path = "../../mila-embedded/src/mcu/vsr.h";
    let source_path = "../../mila-embedded/src/mcu/vsr.c";

    fs::write(header_path, h).expect("Unable to write header file");
    fs::write(source_path, c).expect("Unable to write source file");

    println!("Successfully generated VSR code:");
    println!("  Header: {}", header_path);
    println!("  Source: {}", source_path);
}