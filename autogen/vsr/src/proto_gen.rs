use std::{fs, path::Path};

use anyhow::Result;
use askama::Template;
use indexmap::IndexMap;

use crate::schema::{FieldType, VsrSubstruct};

mod filters {
    use askama::Result as AskamaResult;

    use crate::schema::FieldType;

    /// Maps schema field types onto protobuf scalar kinds used in generated `.proto` files.
    /// Askama filters must return `Result`, so we wrap this infallible mapping in `Ok(...)`.
    pub fn proto_scalar_type(kind: &FieldType) -> AskamaResult<&'static str> {
        Ok(match kind {
            FieldType::Bool => "bool",
            FieldType::I16 | FieldType::I32 => "sint32",
            FieldType::U8 | FieldType::U16 | FieldType::U32 => "uint32",
            FieldType::F32 => "float",
            FieldType::Enum { .. } => "bytes",
        })
    }

    /// Converts arbitrary aliases/field names into PascalCase for protobuf message/type names.
    /// Non-alphanumeric separators break words; leading digits are prefixed to keep identifiers valid.
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

    /// Normalizes arbitrary strings into protobuf-safe snake_case identifiers.
    /// Invalid characters become `_`, repeated separators are collapsed, and empty names get a fallback.
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

    /// Oneof field numbers must be >= 1, so enum semantic values map to proto tag = value + 1.
    pub fn oneof_variant_tag(value: &u32) -> AskamaResult<u32> {
        Ok(value.saturating_add(1))
    }

    /// Normalizes enum variant identifiers while preserving underscore-separated words.
    pub fn enum_value_ident(variant_name: &str) -> AskamaResult<String> {
        let mut out = String::new();
        for ch in variant_name.chars() {
            if ch.is_ascii_alphanumeric() || ch == '_' {
                out.push(ch.to_ascii_uppercase());
            } else {
                out.push('_');
            }
        }
        while out.contains("__") {
            out = out.replace("__", "_");
        }
        out = out.trim_matches('_').to_string();
        if out.is_empty() {
            out = "UNSPECIFIED".to_string();
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

pub fn write_proto_file(
    substructs: &IndexMap<String, VsrSubstruct>,
    out_path: &Path,
) -> Result<()> {
    let rendered = VsrProtoTemplate { substructs }.render()?;

    if let Some(parent) = out_path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(out_path, rendered)?;
    Ok(())
}
