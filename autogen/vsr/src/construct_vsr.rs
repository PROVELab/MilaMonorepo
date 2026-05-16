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

pub fn vsr_definition_dirs() -> Vec<PathBuf> {
    let manifest_dir = Path::new(env!("CARGO_MANIFEST_DIR"));
    vec![
        manifest_dir.join("defs"),
        manifest_dir.join("../../mila-embedded/src/mcu/motor_h300/vsr_defs"),
    ]
}

pub fn load_all_vsr_substructs() -> Result<IndexMap<String, VsrSubstruct>> {
    let mut indexmap = IndexMap::new();

    for resources_dir in vsr_definition_dirs() {
        if !resources_dir.is_dir() {
            anyhow::bail!(
                "VSR definition dir does not exist: {}",
                resources_dir.display()
            );
        }

        println!("Resources dir {}", resources_dir.display());

        let binding = resources_dir.join("*.toml");
        let pattern = binding.to_string_lossy();

        let mut paths: Vec<PathBuf> = glob(&pattern)
            .expect("Invalid glob pattern")
            .filter_map(Result::ok)
            .collect();

        paths.sort();
        if paths.is_empty() {
            anyhow::bail!(
                "No VSR TOML definitions found in {}",
                resources_dir.display()
            );
        }

        for path in paths {
            let key = path
                .file_stem()
                .ok_or(anyhow::anyhow!("Could not get filestem"))?
                .to_string_lossy()
                .into_owned();
            if indexmap.contains_key(&key) {
                anyhow::bail!(
                    "Duplicate VSR definition key `{}` from {}",
                    key,
                    path.display()
                );
            }
            indexmap.insert(key, load_subtype_from_file(&path)?);
        }
    }

    Ok(indexmap)
}
