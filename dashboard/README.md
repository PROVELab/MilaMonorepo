# MilaFrontend

This project is a cross-platform desktop application built with [Tauri](https://tauri.app/) and [Next.js](https://nextjs.org/). It leverages a modern web frontend (React/Next.js) and a secure, lightweight Rust backend via Tauri. The app is designed to run on Linux and macOS.

## Features
- Next.js frontend for a fast, modern UI
- Tauri for native desktop integration and secure backend
- Rust backend for performance and safety
- 3D graphics and interactive UI (see `src/app/components/VehicleDashboard.tsx`)

---

## Prerequisites

### 1. Install Node.js & npm
- **Linux/macOS:**
  - Recommended: [nvm](https://github.com/nvm-sh/nvm#installing-and-updating)
    ```sh
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
    # Restart your shell, then:
    nvm install --lts
    nvm use --lts
    ```
  - Or use your package manager (e.g. `brew install node` on macOS)

### 2. Install Rust & Cargo
- **Linux/macOS:**
    ```sh
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
    # Follow the prompts, then restart your shell
    ```

### 3. Install Tauri CLI
- **Linux/macOS:**
    ```sh
    cargo install tauri-cli
    ```

---

## Getting Started

1. **Clone the repository:**
    ```sh
    git clone <your-repo-url>
    cd MilaFrontend
    ```

2. **Install JavaScript dependencies:**
    ```sh
    npm install
    ```

3. **Build and run the Tauri app:**
    ```sh
    npm run tauri dev
    ```
    This will start both the Next.js frontend and the Tauri backend, launching the desktop app.

---

## Development

- **Frontend code:** `src/app/`, `public/`
- **Tauri (Rust) backend:** `src-tauri/`
- **3D assets:** `public/`
- **Main 3D UI:** `src/app/components/VehicleDashboard.tsx`

### Useful Commands
- `npm run tauri dev` — Start the app in development mode
- `npm run tauri build` — Build a production release
- `npm run dev` — Start only the Next.js frontend (for web preview)

---

## Troubleshooting
- If you get errors about missing Rust or Cargo, make sure you have installed Rust and restarted your shell.
- If you get errors about missing Node.js or npm, ensure you have installed them and are using the correct version (Node 18+ recommended).
- For Tauri-specific issues, see the [Tauri documentation](https://tauri.app/v1/guides/getting-started/prerequisites/).

---

## License
MIT (or specify your license here)

---

## Credits
- [Tauri](https://tauri.app/)
- [Next.js](https://nextjs.org/)
- [React](https://react.dev/)
- [Three.js](https://threejs.org/)

---

## Notes
- This project targets Linux and macOS. Windows is not officially supported in this README.
- For asset and backend API details, see the code in `src/app/components/VehicleDashboard.tsx` and `backend/`.
