#include "VRRenderThread.h"
#include <QMutexLocker>
#include <QCoreApplication>
#include <QFile>
#include <vtkActor.h>
#include <vtkOpenVRRenderer.h>
#include <vtkOpenVRRenderWindow.h>
#include <vtkOpenVRRenderWindowInteractor.h>
#include <vtkLight.h>
#include <vtkProperty.h>
#include <vtkNew.h>
#include <QColor>
#include <QVector3D>
#include "ModelPartList.h"
#include "ModelPart.h"
#include <vtkCamera.h>
#include <vtkJPEGReader.h>
#include <vtkImageData.h>
#include <vtkTexture.h>
#include <vtkSkybox.h>
#include <vtkCommand.h>
#include <vtkEventData.h>
#include <vtkProp3D.h>
#include <vtkCellPicker.h>

static vtkProp3D* g_hoveredActor = nullptr;

class VRScaleCommand : public vtkCommand {
public:
    static VRScaleCommand* New() { return new VRScaleCommand; }

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        Q_UNUSED(caller);

        if (eventId != vtkCommand::Elevation3DEvent)
            return;

        vtkEventData* eventData = static_cast<vtkEventData*>(callData);
        vtkEventDataDevice3D* deviceData = eventData ? eventData->GetAsEventDataDevice3D() : nullptr;

        if (!deviceData || deviceData->GetDevice() != vtkEventDataDevice::RightController)
            return;

        AbortFlagOn();

        if (!g_hoveredActor)
            return;

        const double* trackpad = deviceData->GetTrackPadPosition();
        double currentScale[3];
        g_hoveredActor->GetScale(currentScale);

        const double newScale = currentScale[0] + (trackpad[1] * 0.0001);
        if (newScale > 0.00001)
            g_hoveredActor->SetScale(newScale, newScale, newScale);
    }
};

class VRHoverCommand : public vtkCommand {
public:
    static VRHoverCommand* New() { return new VRHoverCommand; }

    vtkRenderer* renderer = nullptr;
    vtkProp3D* lastHoveredActor = nullptr;
    vtkNew<vtkCellPicker> picker;

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        Q_UNUSED(caller);

        if (eventId != vtkCommand::Move3DEvent)
            return;

        vtkEventData* eventData = static_cast<vtkEventData*>(callData);
        vtkEventDataDevice3D* deviceData = eventData ? eventData->GetAsEventDataDevice3D() : nullptr;

        if (!deviceData || !renderer || deviceData->GetDevice() != vtkEventDataDevice::RightController)
            return;

        const double* worldPosition = deviceData->GetWorldPosition();
        const double* worldOrientation = deviceData->GetWorldOrientation();
        double pos[3] = { worldPosition[0], worldPosition[1], worldPosition[2] };
        double ori[4] = {
            worldOrientation[0],
            worldOrientation[1],
            worldOrientation[2],
            worldOrientation[3]
        };

        picker->SetTolerance(0.0);
        picker->Pick3DRay(pos, ori, renderer);
        vtkProp3D* targetActor = picker->GetProp3D();

        if (targetActor == lastHoveredActor)
            return;

        if (lastHoveredActor) {
            vtkActor* oldActor = vtkActor::SafeDownCast(lastHoveredActor);
            if (oldActor)
                oldActor->GetProperty()->SetAmbient(0.0);
        }

        if (targetActor) {
            vtkActor* newActor = vtkActor::SafeDownCast(targetActor);
            if (newActor) {
                newActor->GetProperty()->SetAmbientColor(0.0, 0.0, 1.0);
                newActor->GetProperty()->SetAmbient(0.6);
            }
        }

        lastHoveredActor = targetActor;
        g_hoveredActor = targetActor;
    }
};

VRRenderThread::VRRenderThread(QObject* parent)
    : QThread(parent) {
}

VRRenderThread::~VRRenderThread() {
    if (isRunning()) {
        issueCommand(Command::EndRender, 0, QVariant());
        wait();
    }
}

void VRRenderThread::setPartList(ModelPartList* list) {
    m_partList = list;
}

void VRRenderThread::addActorOffline(vtkActor* actor, int partID) {
    if (!actor) return;
    m_pendingActors.emplace_back(partID, vtkSmartPointer<vtkActor>(actor));
}

void VRRenderThread::issueCommand(Command c, int partID, const QVariant& data) {
    QMutexLocker locker(&m_mutex);
    m_commandQueue.enqueue({ c, partID, data, nullptr });
}

void VRRenderThread::run() {
    _putenv_s("VTK_VR_SIMULATOR", "1");

    m_renderer = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    const std::string manifestPath =
        (QCoreApplication::applicationDirPath() + "/vrbindings/vtk_openvr_actions.json").toStdString();
    m_interactor->SetActionManifestFileName(manifestPath.c_str());
    m_interactor->SetActionSetName("/actions/vtk");

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    vtkNew<VRHoverCommand> hoverCommand;
    hoverCommand->renderer = m_renderer;
    hoverCommand->picker->PickFromListOn();

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
        hoverCommand->picker->AddPickList(actor);
    }
    m_pendingActors.clear();

    m_interactor->AddObserver(vtkCommand::Move3DEvent, hoverCommand);

    vtkNew<VRScaleCommand> scaleCommand;
    m_interactor->AddObserver(vtkCommand::Elevation3DEvent, scaleCommand);

    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // Skybox: looks for room.jpg next to the executable.
    QString roomPath = QCoreApplication::applicationDirPath() + "/room.jpg";
    vtkNew<vtkJPEGReader> bgReader;

    if (bgReader->CanReadFile(roomPath.toStdString().c_str())) {
        bgReader->SetFileName(roomPath.toStdString().c_str());
        bgReader->Update();

        vtkNew<vtkTexture> bgTexture;
        bgTexture->SetInputConnection(bgReader->GetOutputPort());
        bgTexture->InterpolateOn();

        vtkNew<vtkSkybox> skybox;
        skybox->SetTexture(bgTexture);
        skybox->SetProjectionToSphere();
        skybox->PickableOff();
        m_renderer->AddActor(skybox);
    }

    m_renderWindow->Initialize();

    m_endRender = false;
    while (!m_endRender) {
        m_renderWindow->Render();
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();
    }

    m_renderWindow->Finalize();
    m_renderer = nullptr;
    m_renderWindow = nullptr;
    m_interactor = nullptr;
}

void VRRenderThread::processCommands() {
    QMutexLocker locker(&m_mutex);
    while (!m_commandQueue.isEmpty()) {
        applyCommand(m_commandQueue.dequeue());
    }
}

void VRRenderThread::applyCommand(const CommandPacket& cmd) {
    switch (cmd.type) {
    case Command::EndRender:
        m_endRender = true;
        break;
    case Command::SetColour: {
        auto it = m_activeActors.find(cmd.partID);
        if (it != m_activeActors.end()) {
            QColor c = cmd.data.value<QColor>();
            it->second->GetProperty()->SetColor(c.redF(), c.greenF(), c.blueF());
        }
        break;
    }
    case Command::SetVisible: {
        auto it = m_activeActors.find(cmd.partID);
        if (it != m_activeActors.end()) {
            it->second->SetVisibility(cmd.data.toBool() ? 1 : 0);
        }
        break;
    }
    case Command::RotateY: {
        auto it = m_activeActors.find(cmd.partID);
        if (it != m_activeActors.end()) {
            it->second->RotateY(cmd.data.toDouble());
        }
        break;
    }
    case Command::SetTransform: {
        auto it = m_activeActors.find(cmd.partID);
        if (it != m_activeActors.end()) {
            QVector3D v = cmd.data.value<QVector3D>();
            it->second->SetPosition(v.x(), v.y(), v.z());
        }
        break;
    }
    case Command::ToggleShrink:
    case Command::ToggleClip: {
        if (!m_partList) break;
        ModelPart* p = m_partList->findByID(cmd.partID);
        if (!p) break;
        p->rebuildVRPipeline();
        break;
    }
    case Command::AddActor:
    case Command::RemoveActor:
        break;
    }
}
