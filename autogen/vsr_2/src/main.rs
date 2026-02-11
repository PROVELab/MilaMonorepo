use std::path::Path;

use crate::{
    construct_vsr::load_all_vsr_substructs,
    proto_gen::{write_proto_file, write_vsr_state_files},
    schema::{FieldType, VsrSubstruct},
};

pub mod construct_vsr;
pub mod proto_gen;
pub mod schema;
use anyhow::Result;
use askama::Template;
use indexmap::IndexMap;

mod filters {
    use askama::Result as AskamaResult;

    use crate::schema::FieldType;

    pub fn c_type(kind: &FieldType) -> AskamaResult<&'static str> {
        Ok(kind.c_type())
    }
}

#[derive(Template)]
#[template(path = "vsr.h.j2", escape = "none", whitespace = "preserve")]
pub struct VsrHeaderTemplate<'a> {
    pub substructs: &'a IndexMap<String, VsrSubstruct>,
}

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

    // Existing VSR stdout print path left disabled intentionally.
    // for (path, info) in &substructs {
    //     info.validate()?;
    //     println!("path {}: {:#?}", path, info);
    // }
    //
    // let tpl = VsrHeaderTemplate {
    //     substructs: &substructs,
    // };
    // let rendered = tpl.render()?;
    // println!("{}", rendered);

    Ok(())
}
