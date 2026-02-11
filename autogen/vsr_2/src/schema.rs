use serde::Deserialize;
use indexmap::IndexMap;
use anyhow::{Result, bail};


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
            bail!("definition `{}` must contain at least one field", self.alias);
        }

        for (name, field) in &self.fields {
            if let FieldType::Enum { variants } = &field.kind {
                if variants.is_empty() {
                    bail!("enum field `{}` must have at least one variant", name);
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
    // TODO: add more
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
            FieldType::Enum { .. } => "/* enum */ int",
        }
    }
}

#[derive(Debug, Deserialize, Clone)]
pub struct EnumVariantPayload {
    #[serde(flatten)]
    pub fields: IndexMap<String, FieldType>,
}
