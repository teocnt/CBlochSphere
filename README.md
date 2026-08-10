# sferaBloch (`sbl`)

An interactive **Bloch sphere** visualizer written in plain C with SDL2.

The program renders a wireframe sphere in perspective projection, rotatable with the mouse, plus a state vector representing the state of a single qubit. Keyboard input applies quantum gates (Pauli X/Y/Z and 45° rotations) so you can watch the vector move across the sphere.

Repo: <https://github.com/teocnt/CBlochSphere>

---

## Contents

- [Controls](#controls)
- [The math behind it](#the-math-behind-it)
  - [Axis convention](#axis-convention)
  - [Polar ↔ Cartesian conversion](#polar--cartesian-conversion)
  - [Sphere mesh generation](#sphere-mesh-generation)
  - [3D rotations](#3d-rotations)
  - [Perspective projection](#perspective-projection)
  - [Depth ordering (back/front)](#depth-ordering-backfront)
  - [Thick lines](#thick-lines)
  - [Quantum gates](#quantum-gates)
- [Installation and build](#installation-and-build)
- [Cross-platform compatibility](#cross-platform-compatibility)
- [Project layout](#project-layout)
- [Notes and known limits](#notes-and-known-limits)

---

## Controls

| Input | Action |
|---|---|
| Left-click drag | Rotate the sphere (yaw/pitch) |
| `X` | Apply the Pauli **X** gate |
| `Y` | Apply the Pauli **Y** gate |
| `Z` | Apply the Pauli **Z** gate |
| `Shift` + `X` / `Y` / `Z` | **45°** rotation about the corresponding axis |
| `R` | Reset the state to \|0⟩ |
| `F` | Toggle fullscreen (recomputes centering offsets) |
| `Esc` or window close | Quit |

Every time a gate is applied, the new state `(theta, phi)` is printed to stdout.

---

## The math behind it

### Axis convention

The code uses a "graphics" convention in which **the polar axis is `y`** (vertical on screen) and `z` is the depth axis, pointing towards the viewer. The mapping to Bloch sphere axes is therefore:

| Bloch sphere | Axis in code | On-screen direction |
|---|---|---|
| Z (the \|0⟩–\|1⟩ axis) | `y` | vertical |
| X | `x` | horizontal |
| Y | `z` | depth |

The qubit state is stored directly as a pair of angles in `Polar2 { theta, phi }`, where `theta ∈ [0, π]` is the polar angle measured from the north pole (`|0⟩`, i.e. `+y`) and `phi ∈ [0, 2π)` is the azimuth in the `x–z` plane. The initial state is `(0, 0)` = `|0⟩`.

This parametrization matches the standard form of a pure single-qubit state:

```
|ψ⟩ = cos(θ/2) |0⟩ + e^(iφ) sin(θ/2) |1⟩
```

where `θ` and `φ` are exactly the spherical angles of the Bloch vector.

### Polar ↔ Cartesian conversion

`fromPolarToCartesian` (unit radius, polar axis `y`):

```
y = cos θ
x = sin θ · cos φ
z = sin θ · sin φ
```

`fromCartesianToPolar` is its inverse:

```
θ = acos(clamp(y, -1, 1))
φ = atan2(z, x)          → mapped into [0, 2π) by adding 2π when negative
```

**Clamping** `y` to `[-1, 1]` before `acos` is essential: after a chain of floating-point rotations the value can drift marginally outside the domain, which would yield `NaN`.

### Sphere mesh generation

The sphere is a grid of parallels and meridians built by uniformly sampling angle space:

- `NPAR = 12` parallels, step `Δθ = π / (NPAR − 1)` → both poles included;
- `NMER = 24` meridians, step `Δφ = 2π / NMER` → cyclic closure.

Vertex count:

```
nPoints = NPAR · NMER = 288
```

Points are stored in *row-major* order: the index of vertex `(i, j)` is `i·NMER + j`.

Edges are generated in `calculateSphereConnections`:

- one arc **along the parallel**, for every point: `(i, j) → (i, (j+1) mod NMER)` — the modulo closes the ring;
- one arc **along the meridian**, only when `i < NPAR − 1`: `(i, j) → (i+1, j)`.

Hence:

```
nConnections = NPAR·NMER + (NPAR−1)·NMER = 288 + 264 = 552
```

Both counts are stored in the `Model` struct: the generation routines derive their loop bounds from `NPAR`/`NMER`, while `nPoints` and `nConnections` drive the per-frame loops in `drawSphere` (rotation, projection and edge drawing) and the size of the allocations.

### 3D rotations

Rotations are implemented as elementary rotation matrices applied component-wise (no linear algebra library involved).

**About Y** (`rotateYVec3`):

```
x' =  x·cos α + z·sin α
y' =  y
z' = −x·sin α + z·cos α
```

**About X** (`rotateXVec3`):

```
x' =  x
y' =  y·cos α − z·sin α
z' =  y·sin α + z·cos α
```

**About Z** (`rotateZVec3`):

```
x' =  x·cos α − y·sin α
y' =  x·sin α + y·cos α
z' =  z
```

In the render path the composition order is **fixed**: yaw about `y` first, then pitch about `x`.

```
p_rot = Rx(xRotationAngle) · Ry(yRotationAngle) · p
```

Camera angles accumulate from mouse deltas (`xrel`, `yrel`) scaled by the sensitivity `rotationSense = 0.01` rad/pixel, and are wrapped back into `[0, 2π)` with `fmod` plus a sign correction, so they never grow unbounded and lose precision.

### Perspective projection

A classic perspective projection with the center of projection on the `z` axis at distance `PRESPECTIVE = 3.0`:

```
x_2D = x / (z + d)
y_2D = y / (z + d)          with d = 3.0
```

Since the sphere has radius 1, the denominator stays within `[2, 4]`: never zero, so there is no singular division and no point falls behind the projection plane. The result is then mapped to screen coordinates, with the vertical axis flipped (in SDL, `y` grows downwards):

```
X = round(x_2D · SCALE + xOffset)
Y = round(yOffset − y_2D · SCALE)
```

with `SCALE = 600` and offsets initially at the window center (`1000×900`), recomputed when switching to fullscreen.

### Depth ordering (back/front)

With no z-buffer available, depth is handled by a simplified **painter's algorithm** in two passes:

1. `drawSphere(&sphere, false)` draws edges whose `zMid ≤ 0` (rear hemisphere) in dark grey;
2. the state vector is drawn;
3. `drawSphere(&sphere, true)` draws edges whose `zMid > 0` (front hemisphere) in semi-transparent white.

The classification criterion is the edge's midpoint in `z` after rotation, `(z_start + z_end) / 2`. The effect is that the state vector correctly appears in front of the rear face and behind the front one.

### Thick lines

SDL2 can only draw 1-pixel lines. `drawThickLine` builds a thick line **recursively**: at each level it draws two parallel lines offset by `±(t−1)` pixels and calls itself with `t−1`, down to the base case `t = 0`. The offset is applied along the axis *orthogonal* to the dominant one (`|dx| > |dy|` → vertical offset, otherwise horizontal), so the thickness is always perpendicular to the line direction.

### Quantum gates

The Pauli gates are rotations by **π** about the Bloch sphere axes, and are implemented as closed-form transformations directly on the angles `(θ, φ)` — without going through Cartesian coordinates:

| Gate | Angle transformation | Effect on the Bloch vector |
|---|---|---|
| **Z** | `φ → (φ + π) mod 2π` | `(x, y, z) → (−x, y, −z)` — π rotation about the polar axis |
| **X** | `θ → π − θ`, `φ → −φ` (wrapped into `[0, 2π)`) | `(x, y, z) → (x, −y, −z)` — π rotation about the `x` axis |
| **Y** | `θ → π − θ`, `φ → π − φ` | `(x, y, z) → (−x, −y, z)` — π rotation about the code's `z` axis (= Bloch Y) |
| **Reset** (`R`) | `θ = 0`, `φ = 0` | returns the state to \|0⟩ |

Sanity check for the X gate: `θ → π − θ` gives `cos θ → −cos θ` (so `y → −y`) while `sin θ` is unchanged; `φ → −φ` leaves `cos φ` unchanged (`x → x`) and flips `sin φ` (`z → −z`). All three components confirm a π rotation about `x`.

The **45° rotations** (`apply45RotationGate`, triggered with `Shift`) take the general route instead, since they have no comparably compact closed form in angle space:

```
polar → Cartesian → π/4 rotation matrix → Cartesian → polar
```

The logical-axis → code-axis mapping follows the convention in the table above: `'X'` uses `rotateXVec3`, `'Y'` uses `rotateZVec3`, `'Z'` uses `rotateYVec3`.

> A note on angle conventions: rotating the **Bloch vector** by `α` corresponds to the gate `R_n(α) = exp(−i·α·σ_n/2)`, i.e. an angle of `α/2` in Hilbert space. The 45° rotations implemented here are therefore `R(π/4)` gates — for the Z axis, the **T** gate.

---

## Installation and build

### Dependencies

- a C compiler (C99 or later): **GCC**, **Clang** or **MinGW-w64**
- **SDL2** (development package, not just the runtime)
- **libm** (standard math library, usually already present)
- `make` (optional: the build is a single command line)

### Clone the repository

```bash
git clone https://github.com/teocnt/CBlochSphere.git
cd CBlochSphere
```

### Building with the Makefile

The bundled `Makefile` targets **macOS on Apple Silicon** (Homebrew paths under `/opt/homebrew`):

```makefile
CC      = gcc
CFLAGS  = -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lSDL2
TARGET  = sbl
```

```bash
make          # produces the ./sbl executable
make clean    # removes the executable
```

On other platforms you can simply override the variables on the command line, without editing the file:

```bash
make CFLAGS="$(sdl2-config --cflags)" LDFLAGS="$(sdl2-config --libs) -lm"
```

### Manual build (portable)

The most robust approach on any system is letting `sdl2-config` (or `pkg-config`) resolve the paths:

```bash
gcc sferaBL.c -o sbl $(sdl2-config --cflags) $(sdl2-config --libs) -lm
```

or:

```bash
gcc sferaBL.c -o sbl $(pkg-config --cflags --libs sdl2) -lm
```

Recommended flags during development:

```bash
gcc -std=c99 -Wall -Wextra -O2 sferaBL.c -o sbl $(sdl2-config --cflags --libs) -lm
```

- `-lm` is **required** on Linux with older glibc versions (`sin`, `cos`, `acos`, `atan2`, `fmod`, `round`); on macOS it is harmless, since libm is part of libSystem.
- `-O2` is worth using: the render loop recomputes 288 points and 552 edges twice per frame.

### Running

```bash
./sbl
```

The program takes no command-line arguments and opens a `1000×900` window directly.

---

## Cross-platform compatibility

The code is **portable at the source level**: it uses only standard C, `math.h` and SDL2, with no platform-specific system calls. The only non-portable piece in the repository is the `Makefile`, which is tuned for Homebrew on ARM.

### macOS

**Apple Silicon (M1/M2/M3/M4)** — the Makefile's default configuration.

```bash
brew install sdl2
make
```

Homebrew installs under `/opt/homebrew`, exactly the paths already in the Makefile.

**Intel (x86_64)** — Homebrew installs under `/usr/local`, so the paths need overriding:

```bash
brew install sdl2
make CFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib -lSDL2"
```

Notes:

- on macOS `gcc` is typically an alias for **Clang**; the code compiles fine with either;
- `M_PI` is available from `math.h` by default on macOS;
- to ship a `.app` bundle, SDL2 expects main to be handled by `SDLmain`; for terminal use as here, nothing special is needed;
- on first launch macOS may ask for screen-access permissions or show a Gatekeeper warning for an unsigned binary.

### Linux

The most straightforward target. Install the development packages:

```bash
# Debian / Ubuntu / Linux Mint
sudo apt install build-essential libsdl2-dev

# Fedora / RHEL
sudo dnf install gcc make SDL2-devel

# Arch / Manjaro
sudo pacman -S base-devel sdl2

# openSUSE
sudo zypper install gcc make libSDL2-devel
```

Build:

```bash
gcc sferaBL.c -o sbl $(sdl2-config --cflags --libs) -lm
```

Notes:

- `-lm` is needed on distros with glibc < 2.35 (from glibc 2.35 onwards libm is merged into libc, but the flag stays harmless);
- `M_PI` is exposed by `math.h` with POSIX/GNU extensions enabled by default; compiling with **strict** `-std=c99` may hide it — use `-std=gnu99` or `-D_GNU_SOURCE`;
- on **Wayland**, SDL2 works both natively and through XWayland; if rendering misbehaves, force the backend with `SDL_VIDEODRIVER=x11 ./sbl`;
- the renderer is created with `SDL_RENDERER_ACCELERATED`: on machines without suitable GPU drivers (VMs, headless servers) creation can fail and the program exits with an error message. In that case force the software fallback with `SDL_RENDER_DRIVER=software ./sbl`;
- fullscreen uses `SDL_WINDOW_FULLSCREEN_DESKTOP`, whose behaviour on Linux depends on the window manager; some tiling WMs may ignore it.

### Windows

No ready-made `Makefile`: the simplest path is **MSYS2 + MinGW-w64**.

```bash
# from the "MSYS2 MINGW64" shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkg-config make
gcc sferaBL.c -o sbl.exe $(pkg-config --cflags --libs sdl2) -lm
```

To keep a console window attached (useful, since the program prints the state to stdout), do **not** pass `-mwindows`. Under MinGW, SDL remaps `main` onto `SDL_main`: if you get link errors mentioning `WinMain`, add `-lmingw32 -lSDL2main -lSDL2` **in that order**.

Alternative with **standalone MinGW-w64** and SDL2 downloaded manually from <https://libsdl.org>:

```bash
gcc sferaBL.c -o sbl.exe -IC:\SDL2\include -LC:\SDL2\lib -lmingw32 -lSDL2main -lSDL2 -lm
```

`SDL2.dll` (from `lib\x64`) must be copied next to `sbl.exe`, otherwise the program will not start.

**MSVC (Visual Studio) — requires source changes**, because of two known incompatibilities:

1. `M_PI` is not defined in MSVC's `math.h` without `#define _USE_MATH_DEFINES` before `#include <math.h>` (or `/D_USE_MATH_DEFINES` on the command line);
2. `drawSphere` uses **VLAs** (variable-length arrays: `Vec2 projectedPoints[sphere->model.nPoints]`), a C99 feature MSVC does not support. A fixed-size array or `malloc` would be needed.

For those reasons **MinGW/MSYS2 is the recommended toolchain on Windows**. Building through WSL2 also works, following the Linux instructions (with WSLg providing the window).

### Compatibility summary

| System | Toolchain | Status | Notes |
|---|---|---|---|
| macOS Apple Silicon | Clang/GCC + Homebrew | ✅ default configuration | Makefile ready to use |
| macOS Intel | Clang/GCC + Homebrew | ✅ | override paths with `/usr/local` |
| Linux (X11) | GCC/Clang | ✅ | use `sdl2-config`, add `-lm` |
| Linux (Wayland) | GCC/Clang | ✅ | optional `SDL_VIDEODRIVER=x11` fallback |
| Windows MSYS2/MinGW | GCC | ✅ | mind link flag order, ship `SDL2.dll` |
| Windows MSVC | cl.exe | ⚠️ | needs `_USE_MATH_DEFINES` and VLA removal |
| WSL2 | GCC | ✅ | requires WSLg or an X server |
| BSD | Clang | ✅ (untested) | `pkg-config sdl2` |

---

## Project layout

```
.
├── Makefile      # single-rule build, Homebrew ARM paths
├── sferaBL.c     # single source file: model, math, rendering, input
├── .gitignore    # ignores the "sbl" executable
└── README.md
```

The source is organized into functional blocks:

- **types** — `Polar2`, `Vec2`, `Vec3`, `Connection`, `Model`, `Sphere`;
- **conversions** — `fromPolarToCartesian`, `fromCartesianToPolar`;
- **model construction** — `calculateSphereModel`, `calculateSpherePoints`, `calculateSphereConnections`;
- **3D algebra** — `rotateXVec3`, `rotateYVec3`, `rotateZVec3`;
- **rendering** — `projectsP3`, `drawSphere`, `drawSystemVector`, `drawThickLine`;
- **quantum gates** — `applyXGate`, `applyYGate`, `applyZGate`, `apply45RotationGate`, `resetState`;
- **main** — SDL initialization, event loop, draw loop.

Tunable parameters via `#define` at the top of the file:

| Macro | Value | Meaning |
|---|---|---|
| `WINDOW_WIDTH` / `WINDOW_HEIGHT` | 1000 / 900 | window size |
| `SCALE` | 600 | projection scale factor |
| `PRESPECTIVE` | 3.0 | distance of the center of projection |
| `NPAR` | 12 | number of parallels |
| `NMER` | 24 | number of meridians |
| `RADIUS` | 1 | sphere radius (declared; the mesh is always built on the unit sphere) |

---

## Notes and known limits

- **Degenerate poles**: at `θ = 0` and `θ = π` the `NMER` points of a parallel collapse onto the same vertex, so zero-length arcs get drawn at the poles. The cost is negligible, but it explains why the number of genuinely distinct vertices is `(NPAR−2)·NMER + 2 = 242` rather than 288.
- **Unthrottled loop**: the renderer is created without VSync and there is no frame delay, so the loop runs as fast as it can and keeps CPU/GPU busy. Adding `SDL_RENDERER_PRESENTVSYNC` (or an `SDL_Delay`) would cap it.
- **Unit radius**: the `radius` field of `Sphere` (and the `RADIUS` macro) is never read — the model is always generated on the unit sphere, and apparent size is controlled by `SCALE` alone.
- **Fullscreen offsets**: centering offsets are recomputed by calling `SDL_GetWindowSize` right after `SDL_SetWindowFullscreen`. On some platforms the new size is not reported immediately, so the drawing can stay off-center until the next toggle; handling `SDL_WINDOWEVENT_SIZE_CHANGED` would be more robust. Note also that `SCALE` is not rescaled, so the sphere keeps its pixel size in fullscreen.
- **Physical model**: only the **pure state** of a single qubit is represented (a vector on the sphere's surface). Mixed states, which live inside the sphere, and multi-qubit systems are not modelled.
