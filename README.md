# VR Base Station

A Windows desktop application for viewing Formula Student CAD models in
virtual reality. Built with Qt 6, VTK 9 and OpenVR for the HTC Vive Pro 2.

## Project overview

The VR Base Station loads STL CAD files of a Formula Student race car,
displays them in a 3D viewport with a tree-organised list of editable
parts, and launches a VR session that mirrors the model in a virtual
environment. Users can change colours, toggle visibility, apply shrink
and clip filters, and trigger an explode-view animation. Changes update
live in VR without restarting the headset.

## Features

- Load multiple STL files into a structured tree view
- Per-part property editing: name, colour, visibility
- Shrink and clip filters applicable to any part, in any combination
- Live VR session with skybox and lighting via OpenVR + VTK
- Threaded VR rendering: GUI changes appear in VR within one frame
- Explode-view animation pulling parts smoothly outward from the model centre
- Windows installer via NSIS, bundling Qt, VTK and OpenVR runtimes

## Building from source

```bash
git clone https://github.com/2025-GROUP-1/2025_GROUP_1.git
cd 2025_GROUP_1
cmake -B build -G "Visual Studio 17 2022" -A x64 \
    -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64;C:/path/to/VTK"
cmake --build build --config Release
```

Requirements: Visual Studio 2022 or later, Qt 6.10+, VTK 9 with OpenVR
support, OpenVR SDK, CMake 3.20+.

## Architecture

The application uses two parallel VTK render pipelines: one for the GUI
viewport on the main thread, and one for the VR headset on a dedicated thread.
Each `ModelPart` owns separate actors for each renderer, since VTK
actors cannot be shared between renderers. Cross-thread communication
uses a mutex-protected command queue inside `VRRenderThread`.

For full architectural detail see the
[generated documentation](https://2025-group-1.github.io/2025_GROUP_1/).

## Team

EEEE2076 Group 1, University of Nottingham, Spring 2026.

| Role | Member |
|------|--------|
| GUI & Installer Co-Lead | Ashvath |
| GUI & Installer Co-Lead | Joseph |
| VTK & VR Threading Lead | Senthil |
| Documentation, Doxygen & Build Lead | Hamza |

## Licence

See `LICENSE.txt`.
