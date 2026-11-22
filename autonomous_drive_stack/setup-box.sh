#!/usr/bin/env bash
# Run INSIDE the ros2 distrobox (only needed if you created it without distrobox.ini)
set -e

sudo apt update && sudo apt install -y \
    `# compilers + build` \
    build-essential cmake ninja-build ccache \
    git curl wget zip unzip tar xz-utils bzip2 \
    pkg-config gfortran \
    `# vcpkg fallbacks` \
    flex bison autoconf automake autoconf-archive libtool \
    `# C/C++ tooling` \
    gdb valgrind strace \
    clangd-15 clang-format-15 clang-tidy-15 \
    `# python` \
    python3 python3-pip python3-venv \
    `# shell + everyday utils` \
    zsh tmux htop tree jq less file lsof rsync man-db \
    bash-completion openssh-client ca-certificates gnupg \
    `# networking` \
    iproute2 iputils-ping net-tools \
    `# editor helpers` \
    ripgrep fd-find xclip wl-clipboard

sudo ln -sf /usr/bin/clangd-15 /usr/local/bin/clangd
sudo ln -sf /usr/bin/clang-format-15 /usr/local/bin/clang-format
sudo ln -sf /usr/bin/clang-tidy-15 /usr/local/bin/clang-tidy
sudo ln -sf /usr/bin/fdfind /usr/local/bin/fd

# Newer Neovim (apt's 0.6 is too old)
curl -L https://github.com/neovim/neovim/releases/latest/download/nvim-linux-x86_64.tar.gz |
    sudo tar xz -C /opt
sudo ln -sf /opt/nvim-linux-x86_64/bin/nvim /usr/local/bin/nvim

echo
echo "nvim:   $(nvim --version | head -1)"
echo "clangd: $(clangd --version | head -1)"
echo "cmake:  $(cmake --version | head -1)"
