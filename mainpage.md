@mainpage VR Base Station

# VR Base Station

**Qt 6 · VTK 9 · OpenVR** — Formula Student CAD viewer for the HTC Vive Pro 2

*EEEE2076 Group 1 · University of Nottingham · Spring 2026*

---

## Overview {#overview}

The **VR Base Station** loads STL mesh files of a Formula Student race car, displays them in a
3D viewport alongside a tree-organised list of editable parts, and launches a live VR session
that mirrors the model inside a virtual environment.  Every change made in the GUI — colour,
visibility, filter settings — appears in the headset within one frame, with no headset restart
needed.

---

## Features {#features}

| Feature | Description |
|---------|-------------|
| **STL File Loading** | Import multiple STL files into a structured tree view; each file gets its own VTK pipeline. |
| **Per-Part Property Editing** | Rename parts, change their RGB colour, and toggle visibility without reloading geometry. |
| **Shrink & Clip Filters** | Apply VTK shrink and clip filters to any part, individually or in any combination. |
| **Explode-View Animation** | Smooth outward expansion from the model centre — great for VR assembly walkthroughs. |
| **Live VR Session** | OpenVR session via HTC Vive Pro 2 with skybox lighting, updating in real time as the GUI changes. |
| **Threaded VR Renderer** | GUI on the main thread; VR in a dedicated `VRRenderThread` with a mutex-protected command queue. |
| **Windows Installer** | NSIS installer bundles Qt, VTK, and OpenVR runtimes — no prerequisites for the end user. |
| **Modern Build System** | CMake 3.20+ with VTK module autoinit, Qt6 MOC/UIC, and post-build asset copying. |

---

## Architecture {#architecture}

Two parallel VTK render pipelines keep the GUI viewport and the VR headset independent (VTK actors cannot be shared between renderers).

    Main thread                                VRRenderThread (dedicated)
    ────────────────────────────────           ────────────────────────────────────
    Qt event loop                              Continuous VR render loop
      │                                          │
      ├─ QTreeView                               ├─ OpenVR compositor
      │      │                                   │       │
      ├─ ModelPartList (QAbstractItemModel)       ├─ vtkRenderer  (VR)
      │      │                                   │       │
      │  ModelPart ──────────────────────────────►  actor (VR copy)
      │  (actor-GUI)                             │
      ├─ vtkRenderer (GUI viewport)              │
      │                                          │
      └───── mutex-protected command queue ──────┘
                  (GUI changes → VR thread)

### Class Responsibilities

| Class | Responsibility |
|-------|----------------|
| `ModelPart` | Single tree node; owns VTK pipeline (STL reader → mapper → actor) |
| `ModelPartList` | Qt tree model serving the `ModelPart` hierarchy to `QTreeView` |
| `MainWindow` | Top-level window; owns renderer, render window, and part tree |
| `OptionDialog` | Modal property editor for name, colour, and visibility |

---

## Building from Source {#build}

    git clone https://github.com/senthil-zzz/2025_GROUP_1.git
    cd 2025_GROUP_1
    cmake -B build -G "Visual Studio 17 2022" -A x64 ^
        -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64;C:/path/to/VTK"
    cmake --build build --config Release

**Requirements:** Visual Studio 2022+, Qt 6.10+, VTK 9 with OpenVR support, OpenVR SDK, CMake 3.20+.

---

## Team {#team}

| Role | Member |
|------|--------|
| GUI & Installer Lead | Ashvath |
| GUI & Installer Co-Lead | Joseph |
| VTK & VR Threading Lead | Senthil |
| Documentation, Doxygen & Build Lead | Hamza |

---

## Licence

See `LICENSE.txt` in the repository root.
