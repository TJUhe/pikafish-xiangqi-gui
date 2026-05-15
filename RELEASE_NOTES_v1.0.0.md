# Pikafish Xiangqi GUI v1.0.0

Windows x64 release build.

## Highlights

- Win32 desktop Xiangqi GUI.
- Click-to-move gameplay against engine-controlled black side.
- Selectable engines:
  - Built-in alpha-beta search
  - Pikafish
  - Fairy-Stockfish
  - ElephantEye slot
  - ElephantArt slot
  - PX0 slot
- Hint and resign controls.
- Background engine thinking, so the UI remains responsive.
- CMake install/release packaging.

## Included In This ZIP

- `xiangqi.exe`
- Pikafish Windows engine binaries
- `pikafish.nnue`
- Fairy-Stockfish Windows engine binary
- `variants.ini`
- `check_engines.ps1`
- `README.md`

## Quick Start

Extract the ZIP and run:

```powershell
.\xiangqi.exe
```

Optional engine verification:

```powershell
.\check_engines.ps1
```
