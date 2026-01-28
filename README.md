# Mila Monorepo
Mila monorepo is PROVE's monorepo for all code related to the Mila
vehicle (except for embedded code, for now). It includes:
- Dashboard and Reverse camera
- Homebase telem code
- SerDes and Plotting scripts for logged data
- Documentation and a single source of truth
- etc

Essentially all code that is necessary for Mila will eventually end up in this repository.

## Justfile
The justfile provides handy tools/commands to build, run, and generate. 
For example, just `generate_cc_db` allows you to generate compile_commands.json
so that your LSP gets the requisite symbols. `build_embedded` will build all our 
embedded targets. Feel free to use `just --choose` to see and choose targets.

## Structure
```
/
    /.github
    /tools
        /plot
    /dashboard
    /reverse-camera
    /telem
    /doc
        /memos
        /rfc
        /test_result/
    /experiments
        /bms
```

Future:
```
    /embedded
        /mcu
        /vitals
        /imu... etc
    /autonomy
```

### Vehicle Dashboard
The in-vehicle dashboard can be setup and run via `just start_dashboard`

### Telemetry Dashboard
The telemetry (home-base) dashboard can be run via `just start_telem_dashboard`

## Documentation
All new PROVE Memos will now reside in /doc as Markdown for CS and CPE-related
items. This keeps the documentation close to the code, beneficial both for reviewers
and devs

# Dependencies
To use the items in this monorepo, it's recommended that you have:
- [Python](https://www.python.org/)
- [uv](https://docs.astral.sh/uv/)
- [NodeJS](https://nodejs.org/en/download)
- [Rust](https://rust-lang.org/tools/install/)
- [Just](https://just.systems/man/en/)
- Java JDK (Please look at your package manager on how to do this)
- Gradle

To manage build/setup, we use Justfile which is a slightly simpler Makefile.

We recommend using [sdkman](https://sdkman.io/) to install the jdk and gradle.
