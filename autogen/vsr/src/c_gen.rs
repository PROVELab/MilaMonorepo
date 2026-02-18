use std::{fs, path::Path};

use anyhow::Result;
use askama::Template;
use indexmap::IndexMap;

use crate::schema::VsrSubstruct;

#[derive(Template)]
#[template(path = "vsr_state.h.j2", escape = "none", whitespace = "preserve")]
struct VsrStateHeaderTemplate<'a> {
    substructs: &'a IndexMap<String, VsrSubstruct>,
}

struct VsrStateSlot<'a> {
    name: &'a str,
    enum_field_names: Vec<&'a str>,
}

#[derive(Template)]
#[template(path = "vsr_state.c.j2", escape = "none", whitespace = "preserve")]
struct VsrStateSourceTemplate<'a> {
    slots: Vec<VsrStateSlot<'a>>,
}

pub fn write_vsr_state_files(
    substructs: &IndexMap<String, VsrSubstruct>,
    header_out_path: &Path,
    source_out_path: &Path,
) -> Result<()> {
    let slots = substructs
        .iter()
        .map(|(name, sub)| VsrStateSlot {
            name,
            enum_field_names: sub
                .fields
                .iter()
                .filter_map(|(field_name, field)| {
                    (!field.kind.is_scalar()).then_some(field_name.as_str())
                })
                .collect(),
        })
        .collect();

    let header = VsrStateHeaderTemplate { substructs }.render()?;
    let source = VsrStateSourceTemplate { slots }.render()?;

    if let Some(parent) = header_out_path.parent() {
        fs::create_dir_all(parent)?;
    }
    if let Some(parent) = source_out_path.parent() {
        fs::create_dir_all(parent)?;
    }

    fs::write(header_out_path, header)?;
    fs::write(source_out_path, source)?;
    Ok(())
}
