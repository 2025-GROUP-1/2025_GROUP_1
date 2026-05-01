@page how_it_works How It Works

# How It Works

This page traces three concrete scenarios through the codebase to show how the
classes interact.  Read the [Architecture section](@ref architecture) on the main
page first for a high-level overview.

---

## 1. When the User Changes a Colour {#colour-change}

The following steps trace a colour edit from the GUI dialog down to the renderer.

1. **User opens the property editor.**  
   Right-clicking a tree item and choosing *Item Options* triggers
   `MainWindow::on_actionItem_Options_triggered()` (mainwindow.cpp:200).

2. **Selection is validated.**  
   `ui->treeView->currentIndex()` returns the active `QModelIndex`. Its
   `internalPointer()` is cast to `ModelPart*` — this is the part to edit.

3. **Dialog is populated.**  
   An `OptionDialog` is constructed and `OptionDialog::loadFromModelPart(part)` is
   called (optiondialog.cpp:15). This reads `part->getColourR/G/B()` and sets the
   three `QSpinBox` widgets (0–255 each) plus the name `QLineEdit` and visibility
   `QCheckBox`.

4. **User adjusts the spinboxes and clicks OK.**  
   `dialog.exec()` blocks until the user accepts or cancels.

5. **Changes are written back.**  
   On `QDialog::Accepted`, `OptionDialog::saveToModelPart(part)` is called
   (optiondialog.cpp:30):
   - `part->setData(0, name)` — updates the display name in column 0.
   - `part->setColour(r, g, b)` — stores the three RGB integers in `ModelPart::red/green/blue`.
   - `part->setVisible(checked)` — updates `ModelPart::isVisible`, syncs column 1
     to `"true"`/`"false"`, and calls `actor->SetVisibility()` immediately.

6. **Important caveat on colour.**  
   `ModelPart::setColour()` (ModelPart.cpp:92) stores the values but does **not**
   call `actor->GetProperty()->SetColor()` on the live actor. The actor colour is
   only applied inside `ModelPart::loadSTL()` (ModelPart.cpp:79). Consequently, a
   colour change via the dialog takes visual effect only when the part's STL is
   reloaded.  A direct fix would be to add `actor->GetProperty()->SetColor(...)` at
   the end of `setColour()`.

7. **Tree view is refreshed.**  
   `MainWindow` emits `partList->dataChanged(...)` across the full model range, so
   the "Visible?" column in the `QTreeView` refreshes immediately.

8. **Render is rebuilt.**  
   `MainWindow::updateRender()` (mainwindow.cpp:66) is called:
   - `renderer->RemoveAllViewProps()` clears the current frame.
   - `updateRenderFromTree()` recurses through every `ModelPart` and calls
     `renderer->AddActor(part->getActor())` for each non-null actor.

9. **GPU frame is drawn.**  
   `renderWindow->Render()` submits the updated actor list to OpenGL/VTK.

10. **VR thread receives the update (when VR is running).**  
    A mutex-protected command is enqueued on `VRRenderThread`. At the start of the
    next VR frame the thread drains the queue, updates its own copy of the actor
    (the VR actor, separate from the GUI actor), and calls its VR renderer's
    `Render()`. The headset sees the change within one frame.

---

## 2. Why Two Actors per Part {#two-actors}

VTK actors are tightly coupled to a single `vtkRenderer`, which in turn belongs
to a single `vtkRenderWindow`.  The application runs two render windows:

- **GUI viewport** — a `vtkGenericOpenGLRenderWindow` embedded inside the Qt
  widget via `QVTKOpenGLNativeWidget`.
- **VR headset** — a `vtkOpenVRRenderWindow` driven by OpenVR.

Each render window has its own OpenGL context.  A `vtkActor` stores a reference
to the mapper output *and* is tied to the graphics state of its renderer's
context (GPU buffer handles, transform state, visibility flags).  VTK does not
provide a mechanism for sharing a single actor between two renderers.

**What happens if you try to share an actor:**

- Both renderers call `actor->Render(renderer, ...)` concurrently.  The actor's
  cached render state (built during the first `Render` call) is immediately
  invalidated by the second renderer's context, causing corrupted or missing
  geometry in one or both viewports.
- Thread-safety is violated: the GUI main thread and `VRRenderThread` would race
  to modify the same actor object without synchronisation.

**The solution — two actors, one mapper:**

Each `ModelPart` constructs two actors that both point to the same
`vtkPolyDataMapper` (and thus the same geometry data, which is read-only after
`loadSTL()` completes).  Only actor-level state (colour, visibility, transform)
differs between the two copies, so the geometry is not duplicated in memory.

---

## 3. What the Explode Animation Does Internally {#explode-animation}

The explode view pulls every part outward from the model's centre of mass,
revealing internal geometry and assembly structure.  The implementation (present
in the GUI branch, integrated post-merge) follows this sequence:

1. **Compute the model centre.**  
   Iterate over all top-level `ModelPart` nodes and accumulate their actor bounding
   boxes.  The model centre `C` is the centroid of the combined bounding box.

2. **Compute per-part displacement vectors.**  
   For each part, `computeExplodeTarget(part, C)` returns the vector from `C` to
   the part's own bounding-box centre:
   ```
   d = part.centre - C
   ```
   Parts already at `C` (degenerate case) receive a zero displacement.

3. **Animate over N frames.**  
   A `QTimer` fires at ~60 Hz.  On each tick `t` (0.0 → 1.0):
   ```
   applyExplodeOffset(part, t)
       actor->SetPosition(restPosition + t * scale * d)
   ```
   Both the GUI actor and the corresponding VR actor are updated — the GUI actor
   directly on the main thread, the VR actor via a command queued to
   `VRRenderThread`.

4. **Reset.**  
   Calling explode a second time (or clicking *Reset*) runs the same interpolation
   in reverse (`t` from 1.0 → 0.0), smoothly returning all parts to their resting
   positions.
