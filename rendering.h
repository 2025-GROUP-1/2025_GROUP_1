/**
 * @file rendering.h
 *
 * This file contains no compiled code. It exists solely to define the
 * @ref rendering group so that VRRenderThread (integrated post-merge) and
 * any future VTK helper classes can be documented under this module.
 */

/**
 * @defgroup rendering VTK & VR Rendering
 * @brief VTK render pipelines and the dedicated VR render thread.
 *
 * Handles all GPU-side work: OpenGL via VTK for the GUI viewport, and
 * OpenVR via vtkOpenVRRenderWindow for the headset.  VRRenderThread
 * runs a continuous render loop on a dedicated OS thread, receiving GUI
 * updates via a mutex-protected command queue.  Each ModelPart owns
 * separate actors for the GUI renderer and the VR renderer because VTK
 * actors cannot be shared between render windows.
 *
 * @see data_model, how_it_works
 */
