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
    /autogen
    /dashboard
    /mila-embedded
        /src/mcu
        /src/vitals
    /reverse-camera
    /telem-dashboard
```

Future:
```
    /autonomy
```

### Vehicle Dashboard
The in-vehicle dashboard can be setup and run via `just dashboard`
You can runt the vehicle just dashboard in a vscode terminal. it inserts wierd env vars.
You need to be in a regular linux terminal (and have protobuf and nanopb)

### Telemetry Dashboard
The telemetry (home-base) dashboard can be run via `just start_telem_dashboard`

## Documentation
All new PROVE Memos will now reside in /doc as Markdown for CS and CPE-related
items. This keeps the documentation close to the code, beneficial both for reviewers
and devs

## Nix Dev Shell
The repo has a flake for the shared dev toolchain: Rust, Node/Tauri, Python/uv,
PlatformIO, protobuf/nanopb, Java/Gradle, and Just.

```bash
nix develop
just --list
just dashboard
just release_dashboard_linux
just build_embedded_all
```

Dashboard release artifacts land in `target/release/bundle/{deb,rpm}/`.
PlatformIO itself comes from Nix; downloaded board/toolchain packages are stored
in the repo-local `.platformio` cache.

# Dependencies
If you are not using Nix, install these manually:

To use the items in this monorepo, it's recommended that you have:
- [Python](https://www.python.org/)
  - [uv](https://docs.astral.sh/uv/)
  - Platformio (can be pip installed)
- [NodeJS](https://nodejs.org/en/download)
- [Rust](https://rust-lang.org/tools/install/)
- [Just](https://just.systems/man/en/)
- Java JDK (Please look at your package manager on how to do this)
- Gradle
- [Protocol Buffers compiler (`protoc`)](https://protobuf.dev/installation/)
  - Python packages for nanopb codegen: `nanopb`, `protobuf`

To manage build/setup, we use Justfile which is a slightly simpler Makefile.

We recommend using [sdkman](https://sdkman.io/) to install the jdk and gradle.

### VSR Autogen Requirements
For `just autogen` (VSR v2), make sure nanopb tooling is installed for your Python environment:

```bash
pip3 install --user nanopb protobuf
```
