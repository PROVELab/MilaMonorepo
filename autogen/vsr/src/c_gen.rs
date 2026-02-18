use std::{fs, io::ErrorKind, path::Path};

use anyhow::Result;
use askama::Template;
use indexmap::IndexMap;

use crate::schema::{FieldType, VsrSubstruct};

#[derive(Template)]
#[template(path = "vsr_state.h.j2", escape = "none", whitespace = "preserve")]
struct VsrStateHeaderTemplate<'a> {
    substructs: &'a IndexMap<String, VsrSubstruct>,
}

#[derive(Clone)]
struct VsrStateSlot<'a> {
    name: &'a str,
    topic_name_escaped: String,
    wire_type: String,
    enum_field_names: Vec<&'a str>,
    scalar_print_lines: Vec<String>,
    skipped_enum_fields: Vec<&'a str>,
}

#[derive(Template)]
#[template(path = "vsr_state.c.j2", escape = "none", whitespace = "preserve")]
struct VsrStateSourceTemplate<'a> {
    slots: Vec<VsrStateSlot<'a>>,
}

#[derive(Template)]
#[template(path = "vsr_print.c.j2", escape = "none", whitespace = "preserve")]
struct VsrPrintSourceTemplate<'a> {
    slots: Vec<VsrStateSlot<'a>>,
}

fn pascal(value: &str) -> String {
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
    out
}

fn escape_c_string(value: &str) -> String {
    let mut out = String::new();
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            _ => out.push(ch),
        }
    }
    out
}

fn escape_printf_literal(value: &str) -> String {
    escape_c_string(value).replace('%', "%%")
}

fn build_slots<'a>(substructs: &'a IndexMap<String, VsrSubstruct>) -> Vec<VsrStateSlot<'a>> {
    substructs
        .iter()
        .map(|(name, sub)| {
            let mut scalar_print_lines = Vec::new();
            let mut skipped_enum_fields = Vec::new();

            for (field_name, field) in &sub.fields {
                let label = if field.unit.is_empty() {
                    field_name.clone()
                } else {
                    format!("{} ({})", field_name, field.unit)
                };
                let label = escape_printf_literal(&label);

                match field.kind {
                    FieldType::Bool => scalar_print_lines.push(format!(
                        "    printf(\"  {}: %s\\n\", slot.{} ? \"true\" : \"false\");",
                        label, field_name
                    )),
                    FieldType::I16 | FieldType::I32 => scalar_print_lines.push(format!(
                        "    printf(\"  {}: %ld\\n\", (long)slot.{});",
                        label, field_name
                    )),
                    FieldType::U8 | FieldType::U16 | FieldType::U32 => {
                        scalar_print_lines.push(format!(
                            "    printf(\"  {}: %lu\\n\", (unsigned long)slot.{});",
                            label, field_name
                        ))
                    }
                    FieldType::F32 => scalar_print_lines.push(format!(
                        "    printf(\"  {}: %.3f\\n\", (double)slot.{});",
                        label, field_name
                    )),
                    FieldType::Enum { .. } => skipped_enum_fields.push(field_name.as_str()),
                }
            }

            VsrStateSlot {
                name,
                topic_name_escaped: escape_c_string(&sub.alias),
                wire_type: format!("vsr_{}", pascal(&sub.alias)),
                enum_field_names: sub
                    .fields
                    .iter()
                    .filter_map(|(field_name, field)| {
                        (!field.kind.is_scalar()).then_some(field_name.as_str())
                    })
                    .collect(),
                scalar_print_lines,
                skipped_enum_fields,
            }
        })
        .collect()
}

pub fn write_vsr_state_files(
    substructs: &IndexMap<String, VsrSubstruct>,
    header_out_path: &Path,
    source_out_path: &Path,
) -> Result<()> {
    let slots = build_slots(substructs);

    let header = VsrStateHeaderTemplate { substructs }.render()?;
    let source = VsrStateSourceTemplate {
        slots: slots.clone(),
    }
    .render()?;
    let print_source = VsrPrintSourceTemplate { slots }.render()?;

    if let Some(parent) = header_out_path.parent() {
        fs::create_dir_all(parent)?;
    }
    if let Some(parent) = source_out_path.parent() {
        fs::create_dir_all(parent)?;
    }

    let print_out_path = source_out_path.with_file_name("vsr_print.c");
    let legacy_print_out_path = source_out_path.with_file_name("print_vsr.c");

    fs::write(header_out_path, header)?;
    fs::write(source_out_path, source)?;
    fs::write(print_out_path, print_source)?;
    match fs::remove_file(legacy_print_out_path) {
        Ok(()) => {}
        Err(err) if err.kind() == ErrorKind::NotFound => {}
        Err(err) => return Err(err.into()),
    }
    Ok(())
}
