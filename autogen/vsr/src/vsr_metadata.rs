use std::any::TypeId;

// The overarching vsr type
pub struct vsr_type {
    pub version: u32,
    pub subtypes: &'static [vsr_subtype],
}

impl vsr_type {
    pub fn get_size(&self) -> Option<usize> {
        let mut total_size = 0;
        for subtype in self.subtypes {
            for &(_, type_id, _) in subtype.fields {
                total_size += match type_id {
                    id if id == TypeId::of::<u8>() => 1,
                    id if id == TypeId::of::<i8>() => 1,
                    id if id == TypeId::of::<u16>() => 2,
                    id if id == TypeId::of::<i16>() => 2,
                    id if id == TypeId::of::<u32>() => 4,
                    id if id == TypeId::of::<i32>() => 4,
                    id if id == TypeId::of::<f32>() => 4,
                    id if id == TypeId::of::<u64>() => 8,
                    id if id == TypeId::of::<i64>() => 8,
                    id if id == TypeId::of::<f64>() => 8,
                    _ => return None, // Unknown type
                };
            }
        }
        Some(total_size)
    }
}

// A subtype is a substruct within the VSR. Each substruct
// gets its own mutex in the C code to allow for fine-grained locking.
pub struct vsr_subtype {
    pub name: &'static str,
    pub fields: &'static [(&'static str, TypeId, &'static str)], // (field name, field type, description)
}
