use crate::schema::VsrSubstruct;
use anyhow::{self, Context, Result};
use glob::glob;
use indexmap::IndexMap;
use std::{
    fs,
    path::{Path, PathBuf},
};

pub fn load_subtype_from_file(path: &Path) -> Result<VsrSubstruct> {
    let text = fs::read_to_string(path).with_context(
        || format!("Could not open file {}", path.to_str().unwrap()), // ifthat unwrap fails im cooked chat
    )?;
    let def: VsrSubstruct = toml::from_str(&text)
        .with_context(|| format!("Could not parse TOML file {}", path.display()))?;
    def.validate()
        .with_context(|| format!("Invalid VSR definition {}", path.display()))?;
    Ok(def)
}

pub fn load_all_vsr_substructs() -> Result<IndexMap<String, VsrSubstruct>> {
    let mut indexmap = IndexMap::new();

    let resources_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("defs");

    println!("Resources dir {}", resources_dir.display());

    let binding = resources_dir.join("*.toml");
    let pattern = binding.to_string_lossy();

    let mut paths: Vec<PathBuf> = glob(&pattern)
        .expect("Invalid glob pattern")
        .filter_map(Result::ok)
        .collect();

    paths.sort();

    for path in paths {
        indexmap.insert(
            path.file_stem()
                .ok_or(anyhow::anyhow!("Could not get filestem"))?
                .to_string_lossy()
                .into_owned(),
            load_subtype_from_file(&path)?,
        );
    }

    Ok(indexmap)
}
