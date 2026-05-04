/** @file rendering.h
 *  @brief Doxygen group definition for the VTK & VR Rendering module (no compiled code).
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
 */
