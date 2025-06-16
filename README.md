# Invaders 1999

A modern, retro-inspired 2D space shooter game with parallax backgrounds, enemy formations, and arcade-style action.  
Built with C++, OpenGL, and cross-platform libraries.

---

## 🎮 How to Play

- **Move:** `W`, `A`, `S`, `D` or Arrow Keys
- **Shoot:** `Spacebar`
- **Pause/Quit:** `ESC`
- **Restart (after game over):** `R`
- **Skip Level Transition:** `Spacebar`
- **Mouse:** Click on menu buttons to start the game

### Game Objective
- Destroy all enemy formations in each level.
- Survive enemy attacks and avoid enemy bullets.
- Progress through increasingly difficult levels.
- Earn points for each enemy destroyed and bonus for completing levels.
- The game ends when you lose all your lives or complete all levels.

---

## 🪐 Running the Game (Windows)

1. **Download and extract** the `space_shooter_windows.zip` file.
2. **Run** `space_shooter.exe`.
3. **Do not move or delete** the `resources` folder or the DLL files (`libassimp-5.dll`, `soft_oal.dll`) from the extracted directory.

---

## 🛠️ Building from Source

### Prerequisites

- **CMake** (>= 3.10)
- **A C++17 compiler**
- **OpenGL development libraries**
- **GLFW**
- **Assimp**
- **OpenAL-Soft**
- **stb_image** (included)
- **glm** (header-only, included or installable)

#### On Ubuntu/Linux

```bash
sudo apt update
sudo apt install build-essential cmake libglfw3-dev libassimp-dev libopenal-dev libglm-dev
```

#### On Windows (Native Build)

- Use [MSYS2](https://www.msys2.org/) or Visual Studio with vcpkg/conan for dependencies.
- Or, use the provided cross-compilation instructions below from Linux.

---

### 🏗️ Build Instructions

#### Linux Build

```bash
git clone <this-repo-url>
cd invaders
mkdir build
cd build
cmake ..
make
./space_shooter
```

#### Windows Build (Cross-compile from Linux)

1. **Install MinGW-w64:**
   ```bash
   sudo apt install mingw-w64
   ```
2. **Download Windows libraries:**
   - GLFW, OpenAL-Soft, and Assimp precompiled for Windows (see `win-libs/` in this repo for structure).
3. **Configure and build:**
   ```bash
   cmake -S . -B build-win -D CMAKE_TOOLCHAIN_FILE=toolchain-mingw.cmake
   cmake --build build-win
   ```
   The resulting `space_shooter.exe` and required DLLs will be in `build-win/`.

---

## 📦 Packaging for Distribution

- Zip the following for Windows release:
  - `space_shooter.exe`
  - `libassimp-5.dll`
  - `soft_oal.dll`
  - `resources/` folder

---

## 📝 Credits
- **Art & Audio:** See `resources/` for asset licenses and attributions.
- **Libraries:** GLFW, Assimp, OpenAL-Soft, stb_image, GLM

---

## 📄 License

This project is open source. See `LICENSE` for details.

---

Enjoy the game!  
For issues or contributions, open an issue or pull request on GitHub. 
