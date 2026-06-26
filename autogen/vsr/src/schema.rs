use anyhow::{Result, bail};
use indexmap::IndexMap;
use serde::Deserialize;

// Substruct for the VSR (i.e. like high rate motor
// substruct etc)
#[derive(Debug, Deserialize)]
pub struct VsrSubstruct {
    /// Name used for codegen (e.g. C type prefix)
    pub alias: String,

    /// Description of this substruct as a whole
    pub desc: String,

    /// All `[field_name]` tables land here
    #[serde(flatten)]
    pub fields: IndexMap<String, Field>,
}

impl VsrSubstruct {
    pub fn validate(&self) -> Result<()> {
        if self.fields.is_empty() {
            bail!(
                "definition `{}` must contain at least one field",
                self.alias
            );
        }

        for (name, field) in &self.fields {
            if let FieldType::Enum { variants } = &field.kind {
                if variants.is_empty() {
                    bail!("enum field `{}` must have at least one variant", name);
                }

                let mut seen_values = std::collections::HashSet::new();
                for (variant_name, payload) in variants {
                    if !seen_values.insert(payload.value) {
                        bail!(
                            "enum field `{}` has duplicate value {} (at variant `{}`)",
                            name,
                            payload.value,
                            variant_name
                        );
                    }
                }

                let is_pure_enum = variants.values().all(|payload| payload.fields.is_empty());
                if is_pure_enum && !seen_values.contains(&0) {
                    bail!(
                        "pure enum field `{}` must define a variant with value 0 (proto3 default)",
                        name
                    );
                }
            }
        }

        Ok(())
    }
}

// A field for a substruct (rpm, motor voltage, etc)
#[derive(Debug, Deserialize)]
pub struct Field {
    // Engineering Unit (V for voltage, Arm, RPM, etc)
    pub unit: String,

    // Description (gets added as a comment)
    pub desc: String,

    // What type is this: i32, u8, f32, enum, etc
    #[serde(flatten)]
    pub kind: FieldType,
}

#[derive(Debug, Deserialize, Clone)]
#[serde(tag = "type")]
pub enum FieldType {
    #[serde(rename = "bool")]
    Bool,

    #[serde(rename = "i16")]
    I16,

    #[serde(rename = "i32")]
    I32,

    #[serde(rename = "u16")]
    U16,

    #[serde(rename = "u32")]
    U32,

    #[serde(rename = "u8")]
    U8,

    #[serde(rename = "f32")]
    F32,

    #[serde(rename = "string")]
    String,

    #[serde(rename = "string_array")]
    StringArray,

    #[serde(rename = "enum")]
    Enum {
        variants: IndexMap<String, EnumVariantPayload>,
    },
}

impl FieldType {
    pub fn is_scalar(&self) -> bool {
        !matches!(self, FieldType::Enum { .. })
    }

    pub fn c_type(&self) -> &'static str {
        match self {
            FieldType::Bool => "bool",
            FieldType::I16 => "int16_t",
            FieldType::I32 => "int32_t",
            FieldType::U8 => "uint8_t",
            FieldType::U16 => "uint16_t",
            FieldType::U32 => "uint32_t",
            FieldType::F32 => "float",
            FieldType::String => "char*",
            FieldType::StringArray => "char**",
            FieldType::Enum { .. } => "/* enum */ int",
        }
    }

    pub fn is_pure_enum(&self) -> bool {
        match self {
            FieldType::Enum { variants } => {
                variants.values().all(|payload| payload.fields.is_empty())
            }
            _ => false,
        }
    }

    pub fn is_oneof_enum(&self) -> bool {
        matches!(self, FieldType::Enum { .. }) && !self.is_pure_enum()
    }
}

#[derive(Debug, Deserialize, Clone)]
pub struct EnumVariantPayload {
    pub value: u32,

    #[serde(flatten)]
    pub fields: IndexMap<String, FieldType>,
}
