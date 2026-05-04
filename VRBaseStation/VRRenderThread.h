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

class vtkActor;
class vtkOpenVRRenderer;
class vtkOpenVRRenderWindow;
class vtkOpenVRRenderWindowInteractor;
class ModelPartList;

enum class Command {
    EndRender,      ///< Shut down VR loop. data ignored.
    AddActor,       ///< Add actor at runtime. Use CommandPacket.actor.
    RemoveActor,    ///< Remove actor by partID. data ignored.
    SetColour,      ///< data = QColor.
    SetVisible,     ///< data = bool.
    ToggleShrink,   ///< data = bool (flag state).
    ToggleClip,     ///< data = bool (flag state).
    RotateY,        ///< data = double (degrees to add).
    SetTransform    ///< data = QVector3D (position).
};

struct CommandPacket {
    Command type;
    int partID;
    QVariant data;
    vtkSmartPointer<vtkActor> actor;
};

/**
 * @brief Runs the OpenVR render loop on a dedicated thread.
 */
class VRRenderThread : public QThread {
    Q_OBJECT
public:
    explicit VRRenderThread(QObject* parent = nullptr);
    ~VRRenderThread() override;
    void addActorOffline(vtkActor* actor, int partID);
    void issueCommand(Command c, int partID, const QVariant& data);
    void setPartList(ModelPartList* list);
protected:
    void run() override;
private:
    std::vector<std::pair<int, vtkSmartPointer<vtkActor>>> m_pendingActors;
    std::map<int, vtkSmartPointer<vtkActor>> m_activeActors;
    QMutex m_mutex;
    QQueue<CommandPacket> m_commandQueue;
    bool m_endRender = false;
    ModelPartList* m_partList = nullptr;
    vtkSmartPointer<vtkOpenVRRenderer> m_renderer;
    vtkSmartPointer<vtkOpenVRRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkOpenVRRenderWindowInteractor> m_interactor;
    void processCommands();
    void applyCommand(const CommandPacket& cmd);
};

#endif#pragma once
