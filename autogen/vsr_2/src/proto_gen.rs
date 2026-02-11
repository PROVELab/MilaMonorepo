use std::{fs, path::Path};

use anyhow::Result;
use askama::Template;
use indexmap::IndexMap;

use crate::schema::{FieldType, VsrSubstruct};

mod filters {
    use askama::Result as AskamaResult;

    use crate::schema::FieldType;

    pub fn proto_scalar_type(kind: &FieldType) -> AskamaResult<&'static str> {
        Ok(match kind {
            FieldType::Bool => "bool",
            FieldType::I16 | FieldType::I32 => "sint32",
            FieldType::U8 | FieldType::U16 | FieldType::U32 => "uint32",
            FieldType::F32 => "float",
            FieldType::Enum { .. } => "bytes",
        })
    }

    pub fn pascal(value: &str) -> AskamaResult<String> {
        let mut out = String::new();
        for part in value.split(|c: char| !c.is_ascii_alphanumeric()) {
            if part.is_empty() {
                continue;
            }
            let mut chars = part.chars();
            if let Some(first) = chars.next() {
                out.push(first.to_ascii_uppercase());
                for ch in chars {
                    out.push(ch.to_ascii_lowercase());
                }
            }
        }
        if out.is_empty() {
            out.push_str("Unnamed");
        }
        if out.chars().next().is_some_and(|c| c.is_ascii_digit()) {
            out.insert(0, 'M');
        }
        Ok(out)
    }

    pub fn proto_ident(value: &str) -> AskamaResult<String> {
        let mut out = String::new();
        for ch in value.chars() {
            if ch.is_ascii_alphanumeric() || ch == '_' {
                out.push(ch.to_ascii_lowercase());
            } else {
                out.push('_');
            }
        }
        while out.contains("__") {
            out = out.replace("__", "_");
        }
        out = out.trim_matches('_').to_string();
        if out.is_empty() {
            out = "field".to_string();
        }
        if out.chars().next().is_some_and(|c| c.is_ascii_digit()) {
            out.insert(0, '_');
        }
        Ok(out)
    }
}

#[derive(Template)]
#[template(path = "vsr.proto.j2", escape = "none", whitespace = "preserve")]
struct VsrProtoTemplate<'a> {
    substructs: &'a IndexMap<String, VsrSubstruct>,
}

#[derive(Template)]
#[template(path = "vsr_state.h.j2", escape = "none", whitespace = "preserve")]
struct VsrStateHeaderTemplate<'a> {
    substructs: &'a IndexMap<String, VsrSubstruct>,
}

#[derive(Template)]
#[template(path = "vsr_state.c.j2", escape = "none", whitespace = "preserve")]
struct VsrStateSourceTemplate;

pub fn write_proto_file(substructs: &IndexMap<String, VsrSubstruct>, out_path: &Path) -> Result<()> {
    let rendered = VsrProtoTemplate { substructs }.render()?;

    if let Some(parent) = out_path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(out_path, rendered)?;
    Ok(())
}

pub fn write_vsr_state_files(
    substructs: &IndexMap<String, VsrSubstruct>,
    header_out_path: &Path,
    source_out_path: &Path,
) -> Result<()> {
    let header = VsrStateHeaderTemplate { substructs }.render()?;
    let source = VsrStateSourceTemplate.render()?;

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
