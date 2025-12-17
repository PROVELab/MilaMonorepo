
#[macro_export]
macro_rules! vsr_gen {
    (
        version: $version:expr;
        $(
            $item_name:ident {
                $(
                    $field_name:ident<$ty:ty>($units:expr) $desc:expr;
                )*
            }
        )*
    ) => {
        pub static VSR_METADATA_IMPL: crate::vsr_metadata::vsr_type = crate::vsr_metadata::vsr_type {
            version: $version,
            subtypes: &[
                $(
                    crate::vsr_metadata::vsr_subtype {
                        name: stringify!($item_name),
                        fields: &[
                            $(
                                (stringify!($field_name), std::any::TypeId::of::<$ty>(), $desc),
                            )*
                        ],
                    },
                )*
            ],
        };
    };
}
