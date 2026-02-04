
#[macro_export]
macro_rules! vsr_gen {
    (@alias) => {
        None
    };
    (@alias $alias:ident) => {
        Some(stringify!($alias))
    };
    (
        version: $version:expr;
        $(
            $item_name:ident $(as $alias:ident)? {
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
                        alias: crate::vsr_gen!(@alias $($alias)?),
                        fields: &[
                            $(
                                (stringify!($field_name), std::any::TypeId::of::<$ty>(), $units, $desc),
                            )*
                        ],
                    },
                )*
            ],
        };
    };
}
