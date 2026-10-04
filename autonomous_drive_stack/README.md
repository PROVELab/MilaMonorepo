Everything that will run on the AGX Orin, or is part of Isaac Sim Simulation belongs here

# Autonomous Drive Stack

A modular C++ autonomy stack featuring real-time Ouster LiDAR integration, point cloud processing, and sensor capture/playback capabilities.

---

## Prerequisites

Building this stack requires:

1. **System Build Tools** (C++17/23 capable compiler, CMake 3.19+, Ninja or Make, extraction tools).
2. **vcpkg** (Manifest mode handles C++ dependencies including Ceres, Eigen, spdlog, and Ouster SDK requirements).

---

### Linux

#### 1. Install System Dependencies

Run the command corresponding to your distribution:

**Ubuntu 22.04 / 24.04 / Debian:**

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    gfortran
```

**Fedora:**

```bash
sudo dnf install -y \
    @development-tools \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkgconf-pkg-config \
    gcc-gfortran
```

**Arch Linux:**

```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    ninja \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkgconf \
    gcc-fortran
```

#### 2. Install vcpkg

Clone and bootstrap `vcpkg` in your home directory:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
```

---

### macOS (Apple Silicon & Intel)

macOS is supported for algorithm development, visualization, and offline `.pcap` playback.

#### 1. Install System Dependencies

AppleClang does not provide OpenMP by default (required by Ceres and Ouster mapping). Install LLVM OpenMP alongside CMake and Ninja via Homebrew:

```zsh
# Install Xcode Command Line Tools if not already present
xcode-select --install

# Install build dependencies
brew install cmake ninja git curl pkg-config libomp
```

#### 2. Install vcpkg

```zsh
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
```

> **Note on Physical Hardware:** Offline `.pcap` playback works out of the box. Capturing live Ethernet packets directly from a physical Ouster LiDAR requires root permissions for Berkeley Packet Filter devices (`/dev/bpf*`).

---

### Windows (WSL2 Required)

Native Windows compilation is **not supported** due to POSIX networking, threading, and symlink constraints in the autonomy stack. You must use **WSL2** (Ubuntu).

#### 1. Install WSL2

Open PowerShell as Administrator and run:

```powershell
wsl --install -d Ubuntu
```

Restart your computer if prompted by Windows.

#### 2. Setup Inside WSL2

1. Launch your **Ubuntu** terminal.
2. **Important:** Clone this repository inside your Linux home directory (`~/` or `/home/<username>/`), **never** on the Windows mount (`/mnt/c/...`). Storing code on `/mnt/c/` causes symlink failures and 5–10× slower compile times.
3. Follow the [Ubuntu / Debian instructions](#linux) above to install tools and bootstrap `vcpkg`.

#### 3. Live LiDAR Streaming on Windows 11

If connecting to a physical LiDAR via Ethernet, enable mirrored networking so WSL2 receives incoming UDP packets:

Add the following to `C:\Users\<YourUsername>\.wslconfig`:

```ini
[wsl2]
networkingMode=mirrored
```

Apply the change by running this in PowerShell: `wsl --shutdown`.

---

## Getting Started

### 1. Clone the Repository

Clone the repository recursively to pull in all required submodules (including `third_party/ouster-sdk`):

```bash
git clone --recursive <your-repo-url>
cd autonomous_drive_stack
```

If you already cloned without submodules, fetch them manually:

```bash
git submodule update --init --recursive thirdparty/ouster-sdk
```

### 2. Configure Local User Preset

This repository uses `CMakePresets.json` (shared configuration) and `CMakeUserPresets.json` (personal path overrides, gitignored).

Copy the template:

```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
```

Verify that `CMakeUserPresets.json` points to your `vcpkg` location:

```json
{
  "version": 2,
  "configurePresets": [
    {
      "name": "default",
      "inherits": "vcpkg",
      "environment": {
        "VCPKG_ROOT": "$penv{HOME}/vcpkg"
      }
    }
  ]
}
```

### 3. Build the Project

Configure CMake and compile using the default preset:

```bash
# Configure (installs dependencies via vcpkg automatically on first run; takes 5–10 min)
cmake --preset=default

# Compile project binaries
cmake --build build
```

---

## Editor & LSP Configuration

Language servers (`clangd`, `ccls`) require `compile_commands.json` to resolve `#include` paths. This project automatically generates `build/compile_commands.json`.

### Neovim

Avoid root symlink conflicts by letting your LSP read directly from `build/`:

#### Option A: Project `.clangd` (Recommended)

A `.clangd` file in the project root directs the language server to the compilation database:

```yaml
CompileFlags:
  CompilationDatabase: "build"
```

#### Option B: `cmake-tools.nvim`

Set `action = 'lsp'` in your `cmake-tools.nvim` setup so it injects the database into `clangd` directly in memory without creating root symlinks:

```lua
cmake_compile_commands_options = {
  action = 'lsp',
}
```

### VS Code

Open `.vscode/settings.json` in your workspace:

```json
{
  "cmake.copyCompileCommands": null,
  "clangd.arguments": ["--compile-commands-dir=${workspaceFolder}/build"],
  "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json"
}
```

---

## Troubleshooting

### GCC 15 / 16 `-Wtemplate-body` Errors

GCC 15+ enables strict early template body checking by default, which can cause template compilation errors in older library headers. The root `CMakeLists.txt` automatically appends `-Wno-template-body` when GCC 15 or newer is detected.

### Missing Submodules

If the compiler cannot find `<ouster/...>` headers:

```bash
git submodule update --init --recursive thirdparty/ouster-sdk
```

### Resetting Build State

If CMake cache gets corrupted or dependencies need to be reloaded from scratch:

```bash
rm -rf build vcpkg_installed compile_commands.json
cmake --preset=default
```
