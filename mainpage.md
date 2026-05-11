# VR Base Station

@htmlonly
<div class="vr-hero">
  <div class="vr-hero-inner">
    <div class="vr-kicker">Qt 6 / VTK 9 / OpenVR</div>
    <h1>VR Base Station</h1>
    <p>
      A Formula Student CAD viewer for desktop inspection and HTC Vive Pro 2 walkthroughs.
      The GUI and VR session stay in sync while parts are loaded, filtered, coloured and explored.
    </p>
    <div class="vr-hero-actions">
      <a class="vr-button" href="https://github.com/2025-GROUP-1/2025_GROUP_1/releases/latest/download/VRBaseStation-1.0.0-win64.exe">Download Windows EXE</a>
      <a class="vr-button" href="#features">Explore features</a>
      <a class="vr-button secondary" href="classes.html">Browse classes</a>
      <a class="vr-button secondary" href="how_it_works.html">Read how it works</a>
    </div>
    <div class="vr-chip-row">
      <span class="vr-chip">STL loading</span>
      <span class="vr-chip">Threaded VR renderer</span>
      <span class="vr-chip">Explode view</span>
      <span class="vr-chip">NSIS installer</span>
    </div>
  </div>
</div>
@endhtmlonly

@htmlonly
<div class="vr-badge-row">
  <a class="vr-badge" href="https://github.com/2025-GROUP-1/2025_GROUP_1"><span>Repo</span> GitHub</a>
  <a class="vr-badge vr-badge-download" href="https://github.com/2025-GROUP-1/2025_GROUP_1/releases/latest/download/VRBaseStation-1.0.0-win64.exe"><span>Download</span> Windows EXE</a>
  <a class="vr-badge" href="https://github.com/2025-GROUP-1/2025_GROUP_1/releases/latest"><span>Release</span> v1.4.0</a>
  <a class="vr-badge" href="https://github.com/2025-GROUP-1/2025_GROUP_1/actions"><span>Build</span> Actions</a>
  <a class="vr-badge" href="classes.html"><span>Docs</span> Class index</a>
  <a class="vr-badge" href="files.html"><span>Source</span> File browser</a>
</div>
@endhtmlonly

## Quick Start {#quickstart}

@htmlonly
<div class="vr-quickstart">
  <div class="vr-quickstart-header">
    <div>
      <h3>Build the app</h3>
      <p>Clone, configure and build the Windows desktop app.</p>
    </div>
    <div class="vr-quickstart-actions">
      <a class="vr-button" href="https://github.com/2025-GROUP-1/2025_GROUP_1/releases/latest/download/VRBaseStation-1.0.0-win64.exe">Download EXE</a>
      <a class="vr-button secondary" href="https://github.com/2025-GROUP-1/2025_GROUP_1">Open repo</a>
    </div>
  </div>
  <div class="vr-command-list">
    <div class="vr-command">
      <span class="vr-command-label">Clone</span>
      <code>git clone https://github.com/2025-GROUP-1/2025_GROUP_1.git</code>
      <button class="vr-copy-button" data-copy="git clone https://github.com/2025-GROUP-1/2025_GROUP_1.git">Copy</button>
    </div>
    <div class="vr-command">
      <span class="vr-command-label">Configure</span>
      <code>cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64;C:/path/to/VTK"</code>
      <button class="vr-copy-button" data-copy='cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64;C:/path/to/VTK"'>Copy</button>
    </div>
    <div class="vr-command">
      <span class="vr-command-label">Build</span>
      <code>cmake --build build --config Release</code>
      <button class="vr-copy-button" data-copy="cmake --build build --config Release">Copy</button>
    </div>
  </div>
</div>

<div class="vr-callout-grid">
  <div class="vr-callout">
    <h3>Requirements</h3>
    <p>Visual Studio 2022+, Qt 6.10+, VTK 9 with OpenVR support, OpenVR SDK and CMake 3.20+.</p>
  </div>
  <div class="vr-callout">
    <h3>Latest release</h3>
    <p>Final Release v1.4.0 bundles the Windows executable with the Qt, VTK, OpenVR and debug CRT runtime DLLs.</p>
  </div>
  <div class="vr-callout">
    <h3>VR hardware note</h3>
    <p>The VR path targets OpenVR and the HTC Vive Pro 2 setup used for the project demo.</p>
  </div>
</div>
@endhtmlonly

## Overview {#overview}

The **VR Base Station** loads STL mesh files of a Formula Student race car, displays them in a
3D viewport alongside a tree-organised list of editable parts, and launches a live VR session
that mirrors the model inside a virtual environment. Changes made in the GUI, including colour,
visibility and filter settings, appear in the headset within one frame with no headset restart
needed.

@htmlonly
<div class="vr-stat-grid">
  <div class="vr-stat">
    <strong>2</strong>
    <p>render pipelines for desktop and VR</p>
  </div>
  <div class="vr-stat">
    <strong>3</strong>
    <p>main modules: GUI, data model and rendering</p>
  </div>
  <div class="vr-stat">
    <strong>60 Hz</strong>
    <p>target update rhythm for interactive VR changes</p>
  </div>
  <div class="vr-stat">
    <strong>v1.4.0</strong>
    <p>latest Windows runtime bundle release</p>
  </div>
</div>
@endhtmlonly

## Features {#features}

@htmlonly
<div class="vr-card-grid">
  <div class="vr-card">
    <h3>Load and organise CAD</h3>
    <p>Import multiple STL files into a structured tree, with each part feeding its own VTK pipeline.</p>
  </div>
  <div class="vr-card">
    <h3>Edit part properties</h3>
    <p>Rename parts, change RGB colour values and toggle visibility from the Qt property dialog.</p>
  </div>
  <div class="vr-card">
    <h3>Inspect internal detail</h3>
    <p>Use shrink, clip and explode-view controls to separate assemblies and inspect hidden areas.</p>
  </div>
  <div class="vr-card">
    <h3>Mirror changes in VR</h3>
    <p>Send GUI updates to the headset through a mutex-protected command queue.</p>
  </div>
  <div class="vr-card">
    <h3>Package for Windows</h3>
    <p>Build an NSIS installer that bundles Qt, VTK and OpenVR runtimes.</p>
  </div>
  <div class="vr-card">
    <h3>Navigate the codebase</h3>
    <p>Use class pages, grouped topics and source listings to move quickly through the implementation.</p>
  </div>
</div>
@endhtmlonly

## Architecture {#architecture}

Two parallel VTK render pipelines keep the GUI viewport and the VR headset independent
(VTK actors cannot be shared between renderers; see [Why two actors per part](@ref two-actors)).

    Main thread                                  VRRenderThread
    --------------------------------           --------------------------------
    Qt event loop                               Continuous VR render loop
      |                                           |
      +-- QTreeView                               +-- OpenVR compositor
      |      |                                    |      |
      +-- ModelPartList                           +-- vtkRenderer (VR)
      |      |                                    |      |
      |   ModelPart --------------------------->  actor-VR
      |   actor-GUI
      |
      +-- vtkRenderer (GUI viewport)
      |
      +-- mutex-protected command queue ------->  VR thread

### Class Responsibilities

| Class | Module | Responsibility |
|-------|--------|----------------|
| `ModelPart` | @ref data_model | Single tree node; owns VTK pipeline (STL reader to mapper to actor) |
| `ModelPartList` | @ref data_model | Qt tree model serving the `ModelPart` hierarchy to `QTreeView` |
| `MainWindow` | @ref gui | Top-level window; owns renderer, render window, and part tree |
| `OptionDialog` | @ref gui | Modal property editor for name, colour, and visibility |

## Typical Workflow {#workflow}

@htmlonly
<div class="vr-flow">
  <div class="vr-step">
    <span class="vr-step-number">1</span>
    <h3>Load</h3>
    <p>Import STL files and build the model tree.</p>
  </div>
  <div class="vr-step">
    <span class="vr-step-number">2</span>
    <h3>Edit</h3>
    <p>Adjust names, colours, visibility and filters.</p>
  </div>
  <div class="vr-step">
    <span class="vr-step-number">3</span>
    <h3>Explore</h3>
    <p>Open the VR session and inspect the CAD model at scale.</p>
  </div>
  <div class="vr-step">
    <span class="vr-step-number">4</span>
    <h3>Package</h3>
    <p>Build and distribute the Windows installer.</p>
  </div>
</div>
@endhtmlonly

## Topics {#modules}

The codebase is grouped into three thematic topics:

- **@ref gui**: Qt widgets and dialogs (MainWindow, OptionDialog)
- **@ref data_model**: Tree model and CAD part management (ModelPart, ModelPartList)
- **@ref rendering**: VTK render pipelines and VRRenderThread

Use the **Topics** entry in the sidebar to browse grouped class lists.

## Team {#team}

@htmlonly
<div class="vr-team-grid">
  <div class="vr-team-card">
    <strong>Ashvath</strong>
    <div class="vr-team-role">GUI &amp; Installer Co-Lead</div>
    <p>Worked on the Qt interface, installer packaging and user-facing controls.</p>
  </div>
  <div class="vr-team-card">
    <strong>Joseph</strong>
    <div class="vr-team-role">GUI &amp; Installer Co-Lead</div>
    <p>Integrated GUI behaviour with rendering updates and helped polish the app workflow.</p>
  </div>
  <div class="vr-team-card">
    <strong>Senthil</strong>
    <div class="vr-team-role">VTK &amp; VR Threading Lead</div>
    <p>Led the VR render thread, OpenVR setup and headset-side interaction pipeline.</p>
  </div>
  <div class="vr-team-card">
    <strong>Hamza</strong>
    <div class="vr-team-role">Documentation, Doxygen &amp; Build Lead</div>
    <p>Owned the documentation site, Doxygen setup and build/deployment workflow.</p>
  </div>
</div>
@endhtmlonly

## Licence

See `LICENSE.txt` in the repository root.
