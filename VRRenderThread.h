/**
 * @file VRRenderThread.h
 * @brief Declares the threaded OpenVR renderer and command queue used to mirror GUI changes in VR.
 */

#ifndef VR_RENDER_THREAD_H
#define VR_RENDER_THREAD_H

#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QVariant>
#include <vtkSmartPointer.h>
#include <utility>
#include <vector>
#include <map>
#include <QList>
#include <QString>
#include <QColor>
#include <vtkSkybox.h>

class vtkActor;
class vtkOpenVRRenderer;
class vtkOpenVRRenderWindow;
class vtkOpenVRRenderWindowInteractor;
class ModelPartList;

/**
 * @enum Command
 * @brief Commands that the GUI thread can enqueue for the VR render loop.
 */
enum class Command {
    EndRender,
    AddActor,
    RemoveActor,
    SetColour,
    SetVisible,
    ToggleShrink,
    ToggleClip,
    RotateY,
    SetTransform,
    ToggleSkybox
    };

/**
 * @struct CommandPacket
 * @brief Bundles a queued VR command with its target part, payload, and optional actor.
 */
struct CommandPacket {
    Command type;
    int partID;
    QVariant data;
    vtkSmartPointer<vtkActor> actor;
};

/**
 * @class VRRenderThread
 * @brief Dedicated QThread that owns the OpenVR render loop and applies queued scene updates.
 */
class VRRenderThread : public QThread {
    Q_OBJECT
public:
    explicit VRRenderThread(QObject* parent = nullptr);
    ~VRRenderThread() override;

    // queue an actor before the thread starts (main thread only)
    void addActorOffline(vtkActor* actor, int partID);

    // thread-safe: queue a command for the VR loop to pick up
    void issueCommand(Command c, int partID, const QVariant& data);

    // overload that also carries an actor (used by AddActor / RemoveActor)
    void issueCommand(Command c, int partID, vtkActor* actor);

    void setPartList(ModelPartList* list);

signals:
    // emitted from VR thread when in-headset menu changes a part (GUI connects to update itself)
    void partColourChanged(int partID, QColor colour);
    void partShrinkChanged(int partID, bool enabled);
    void partClipChanged(int partID, bool enabled);
    void partVisibilityChanged(int partID, bool visible);

protected:
    // the actual VR render loop
    void run() override;

private:
    // actors waiting to be added when the thread starts
    std::vector<std::pair<int, vtkSmartPointer<vtkActor>>> m_pendingActors;

    // actors currently in the VR scene, keyed by part ID
    std::map<int, vtkSmartPointer<vtkActor>> m_activeActors;

    QMutex m_mutex;
    QQueue<CommandPacket> m_commandQueue;
    bool m_endRender = false;

    ModelPartList* m_partList = nullptr;
    vtkSmartPointer<vtkSkybox> m_skybox;

    // VTK objects (created and destroyed on the VR thread)
    vtkSmartPointer<vtkOpenVRRenderer> m_renderer;
    vtkSmartPointer<vtkOpenVRRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkOpenVRRenderWindowInteractor> m_interactor;
    bool m_hasGarageAnchor = false;
    double m_garageCenterX = 0.0;
    double m_garageFloorY = 0.0;
    double m_garageCenterZ = 0.0;
    double m_spawnX = 0.0;
    double m_spawnY = 0.0;
    double m_spawnZ = 0.0;

    void processCommands();
    void applyCommand(const CommandPacket& cmd);
};

#endif
