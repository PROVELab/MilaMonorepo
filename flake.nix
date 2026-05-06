{
  description = "Mila monorepo development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        linuxTauriDeps = with pkgs; lib.optionals stdenv.isLinux [
          at-spi2-atk
          cairo
          dbus
          gdk-pixbuf
          glib
          glib-networking
          gtk3
          libsoup_3
          librsvg
          pango
          webkitgtk_4_1
          libx11
          libxcursor
          libxi
          libxrandr
        ];
        nanopbPython = pkgs.python312.withPackages (ps: [ ps.protobuf ]);
        nanopbGenerator = pkgs.writeShellApplication {
          name = "protoc-gen-nanopb";
          runtimeInputs = [ nanopbPython pkgs.protobuf ];
          text = ''
            export PYTHONPATH=${pkgs.nanopb.src}/generator''${PYTHONPATH:+:$PYTHONPATH}
            export NANOPB_PB2_TEMP_DIR=''${TMPDIR:-/tmp}
            export PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python
            exec ${nanopbPython}/bin/python ${pkgs.nanopb.src}/generator/protoc-gen-nanopb "$@"
          '';
        };
        devTools = with pkgs; [
          cargo
          clang-tools
          cmake
          git
          gradle
          jdk17
          just
          nanopbGenerator
          nodejs_22
          openssl
          pkg-config
          platformio
          protobuf
          python312
          rustc
          rustfmt
          clippy
          uv
        ];
      in
      {
        devShells.default = pkgs.mkShell {
          packages = devTools ++ linuxTauriDeps;

          LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath linuxTauriDeps;
          shellHook = ''
            export PATH=${nanopbGenerator}/bin:$PATH
            export PROTOC_GEN_NANOPB=${nanopbGenerator}/bin/protoc-gen-nanopb
            export PLATFORMIO_CORE_DIR=''${PLATFORMIO_CORE_DIR:-$PWD/.platformio}
            export UV_PROJECT_ENVIRONMENT=''${UV_PROJECT_ENVIRONMENT:-.venv}
            echo "dev shell: just, node, rust, python/uv, platformio, tauri deps"
          '';
        };

        packages.dashboard-release = pkgs.writeShellApplication {
          name = "dashboard-release";
          runtimeInputs = [ pkgs.just ];
          text = ''
            exec just release_dashboard
          '';
        };

        apps.dashboard-release = {
          type = "app";
          program = "${self.packages.${system}.dashboard-release}/bin/dashboard-release";
        };
      });
}
