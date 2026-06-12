// build.rs

use std::env;
use std::path::PathBuf;

#[path = "../commonBuild.rs"]
mod buildCommon;

use buildCommon::*;


fn main() {
    // Set this value to the name of your sensor node. Should be able to leave everything else the same
    let sensor_name = "precharge";
    build_common(sensor_name);
}