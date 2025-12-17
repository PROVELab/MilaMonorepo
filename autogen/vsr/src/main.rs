pub mod vsr_macro;
pub mod vsr_metadata;

vsr_gen!(
    version: 1;

    ExampleStruct {
        field_one<u32>("cm") "This is the first field.";
        field_two<i16>("mm") "This is the second field.";
    }

    AnotherStruct {
        subitem<f64>("m") "A floating point value.";
    }
);

fn main() {
    println!("Hello, world!");
}
