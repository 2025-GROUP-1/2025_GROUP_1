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
    EndRender,
    AddActor,
    RemoveActor,
    SetColour,
    SetVisible,
    ToggleShrink,
    ToggleClip,
    RotateY,
    SetTransform
};

struct CommandPacket {
    Command type;
    int partID;
    QVariant data;
    vtkSmartPointer<vtkActor> actor;
};

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

#endif
