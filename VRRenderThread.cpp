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
#include <vtkBillboardTextActor3D.h>
#include <vtkTextProperty.h>

#include <chrono>
#include <cmath>

static vtkProp3D* g_hoveredActor = nullptr;
static vtkProp3D* g_selectedActor = nullptr;

static void setActorGlow(vtkProp3D* prop, double ambient, double r, double g, double b)
{
    vtkActor* actor = vtkActor::SafeDownCast(prop);
    if (!actor)
        return;

    actor->GetProperty()->SetAmbientColor(r, g, b);
    actor->GetProperty()->SetAmbient(ambient);
}

static bool isRightControllerEvent(vtkEventDataDevice device)
{
    return device == vtkEventDataDevice::RightController ||
        device == vtkEventDataDevice::Unknown ||
        device == vtkEventDataDevice::Any;
}

static bool isReleaseAction(vtkEventDataAction action)
{
    return action == vtkEventDataAction::Release ||
        action == vtkEventDataAction::Untouch;
}

class VRTrackpadCommand : public vtkCommand {
public:
    static VRTrackpadCommand* New() { return new VRTrackpadCommand; }

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        Q_UNUSED(caller);

        if (eventId != vtkCommand::Elevation3DEvent)
            return;

        vtkEventData* eventData = static_cast<vtkEventData*>(callData);
        vtkEventDataDevice3D* deviceData = eventData ? eventData->GetAsEventDataDevice3D() : nullptr;

        if (!deviceData || !isRightControllerEvent(deviceData->GetDevice()))
            return;

        AbortFlagOn();

        if (!g_selectedActor)
            return;

        const double* trackpad = deviceData->GetTrackPadPosition();
        double currentScale[3];
        g_selectedActor->GetScale(currentScale);

        const double newScale = currentScale[0] + (trackpad[1] * 0.0001);
        if (newScale > 0.00001)
            g_selectedActor->SetScale(newScale, newScale, newScale);

        if (std::fabs(trackpad[0]) > 0.08)
            g_selectedActor->RotateY(trackpad[0] * 2.0);
    }
};

class VRControlsCommand : public vtkCommand {
public:
    static VRControlsCommand* New() { return new VRControlsCommand; }

    vtkRenderer* renderer = nullptr;
    vtkProp3D* lastHoveredActor = nullptr;
    vtkNew<vtkCellPicker> picker;
    vtkBillboardTextActor3D* menuActor = nullptr;
    vtkProp3D* grabbedActor = nullptr;
    double grabOffset[3] = { 0.0, 0.0, 0.0 };
    std::chrono::steady_clock::time_point lastPickTime = std::chrono::steady_clock::now();
    double lastPickPosition[3] = { 0.0, 0.0, 0.0 };
    double lastPickOrientation[4] = { 0.0, 0.0, 0.0, 0.0 };
    bool hasLastPickPose = false;
    bool menuVisible = false;
    bool triggerDown = false;
    bool gripDown = false;

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        Q_UNUSED(caller);

        vtkEventData* eventData = static_cast<vtkEventData*>(callData);
        vtkEventDataDevice3D* deviceData = eventData ? eventData->GetAsEventDataDevice3D() : nullptr;

        if (!deviceData || !renderer || !isRightControllerEvent(deviceData->GetDevice()))
            return;

        const double* worldPosition = deviceData->GetWorldPosition();
        const double* worldOrientation = deviceData->GetWorldOrientation();
        const double* worldDirection = deviceData->GetWorldDirection();

        if (eventId == vtkCommand::Move3DEvent) {
            updateMenuPosition(worldPosition, worldDirection);
            updateGrabbedActor(worldPosition);
            updateHover(worldPosition, worldOrientation);
            return;
        }

        if (eventId == vtkCommand::Select3DEvent) {
            handleSelect(deviceData);
            return;
        }

        if (eventId == vtkCommand::PositionProp3DEvent) {
            handleGrab(deviceData, worldPosition);
            return;
        }

        if (eventId == vtkCommand::Menu3DEvent) {
            handleMenu(deviceData);
            return;
        }

        if (eventId == vtkCommand::Button3DEvent) {
            if (deviceData->GetInput() == vtkEventDataDeviceInput::Trigger)
                handleSelect(deviceData);
            else if (deviceData->GetInput() == vtkEventDataDeviceInput::Grip)
                handleGrab(deviceData, worldPosition);
            else if (deviceData->GetInput() == vtkEventDataDeviceInput::ApplicationMenu)
                handleMenu(deviceData);
            return;
        }
    }

private:
    void updateMenuPosition(const double* worldPosition, const double* worldDirection)
    {
        if (!menuActor || !menuVisible)
            return;

        constexpr double menuDistance = 1.15;
        menuActor->SetPosition(
            worldPosition[0] + worldDirection[0] * menuDistance,
            worldPosition[1] + worldDirection[1] * menuDistance + 0.15,
            worldPosition[2] + worldDirection[2] * menuDistance);
    }

    void updateGrabbedActor(const double* worldPosition)
    {
        if (!grabbedActor)
            return;

        grabbedActor->SetPosition(
            worldPosition[0] + grabOffset[0],
            worldPosition[1] + grabOffset[1],
            worldPosition[2] + grabOffset[2]);
    }

    void updateHover(const double* worldPosition, const double* worldOrientation)
    {
        const auto now = std::chrono::steady_clock::now();
        const double positionDelta =
            std::fabs(worldPosition[0] - lastPickPosition[0]) +
            std::fabs(worldPosition[1] - lastPickPosition[1]) +
            std::fabs(worldPosition[2] - lastPickPosition[2]);
        const double orientationDelta =
            std::fabs(worldOrientation[0] - lastPickOrientation[0]) +
            std::fabs(worldOrientation[1] - lastPickOrientation[1]) +
            std::fabs(worldOrientation[2] - lastPickOrientation[2]) +
            std::fabs(worldOrientation[3] - lastPickOrientation[3]);

        if (hasLastPickPose) {
            const auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPickTime).count();
            if (elapsedMs < 50 || (positionDelta < 0.002 && orientationDelta < 0.01))
                return;
        }

        lastPickTime = now;
        hasLastPickPose = true;
        lastPickPosition[0] = worldPosition[0];
        lastPickPosition[1] = worldPosition[1];
        lastPickPosition[2] = worldPosition[2];
        lastPickOrientation[0] = worldOrientation[0];
        lastPickOrientation[1] = worldOrientation[1];
        lastPickOrientation[2] = worldOrientation[2];
        lastPickOrientation[3] = worldOrientation[3];

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

        if (lastHoveredActor && lastHoveredActor != grabbedActor && lastHoveredActor != g_selectedActor)
            setActorGlow(lastHoveredActor, 0.0, 1.0, 1.0, 1.0);

        if (targetActor && targetActor != grabbedActor && targetActor != g_selectedActor)
            setActorGlow(targetActor, 0.45, 0.1, 0.45, 1.0);

        lastHoveredActor = targetActor;
        g_hoveredActor = targetActor;
    }

    void handleSelect(vtkEventDataDevice3D* deviceData)
    {
        AbortFlagOn();

        if (isReleaseAction(deviceData->GetAction())) {
            triggerDown = false;
            return;
        }

        if (triggerDown)
            return;

        triggerDown = true;

        if (g_selectedActor && (!g_hoveredActor || g_selectedActor == g_hoveredActor)) {
            setActorGlow(g_selectedActor, 0.0, 1.0, 1.0, 1.0);
            g_selectedActor = nullptr;
            return;
        }

        if (!g_hoveredActor)
            return;

        if (g_selectedActor && g_selectedActor != g_hoveredActor)
            setActorGlow(g_selectedActor, 0.0, 1.0, 1.0, 1.0);

        g_selectedActor = g_hoveredActor;
        setActorGlow(g_selectedActor, 0.75, 0.1, 1.0, 0.35);
    }

    void handleGrab(vtkEventDataDevice3D* deviceData, const double* worldPosition)
    {
        AbortFlagOn();

        if (isReleaseAction(deviceData->GetAction())) {
            gripDown = false;
            releaseGrabbedActor();
            return;
        }

        if (!gripDown && g_hoveredActor) {
            gripDown = true;
            grabbedActor = g_hoveredActor;
            double actorPosition[3];
            grabbedActor->GetPosition(actorPosition);
            grabOffset[0] = actorPosition[0] - worldPosition[0];
            grabOffset[1] = actorPosition[1] - worldPosition[1];
            grabOffset[2] = actorPosition[2] - worldPosition[2];
            setActorGlow(grabbedActor, 0.8, 1.0, 0.65, 0.1);
        }
    }

    void releaseGrabbedActor()
    {
        if (grabbedActor)
            setActorGlow(grabbedActor,
                grabbedActor == g_selectedActor ? 0.75 : 0.45,
                grabbedActor == g_selectedActor ? 0.1 : 0.1,
                grabbedActor == g_selectedActor ? 1.0 : 0.45,
                grabbedActor == g_selectedActor ? 0.35 : 1.0);
        grabbedActor = nullptr;
    }

    void handleMenu(vtkEventDataDevice3D* deviceData)
    {
        if (deviceData->GetAction() != vtkEventDataAction::Press || !menuActor) {
            return;
        }

        AbortFlagOn();
        menuVisible = !menuVisible;
        menuActor->SetVisibility(menuVisible ? 1 : 0);
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

    vtkNew<vtkBillboardTextActor3D> menuActor;
    menuActor->SetInput(
        "VR Controls\n"
        "Trigger: select highlighted part\n"
        "Trigger again: deselect\n"
        "Grip: hold on pointed part to move\n"
        "Trackpad up/down: scale selected part\n"
        "Trackpad left/right: rotate selected part\n"
        "Menu: show/hide this panel");
    menuActor->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
    menuActor->GetTextProperty()->SetBackgroundColor(0.04, 0.05, 0.06);
    menuActor->GetTextProperty()->SetBackgroundOpacity(0.85);
    menuActor->GetTextProperty()->SetFontSize(24);
    menuActor->SetScale(0.008, 0.008, 0.008);
    menuActor->PickableOff();
    menuActor->SetVisibility(0);
    m_renderer->AddActor(menuActor);

    vtkNew<VRControlsCommand> controlsCommand;
    controlsCommand->renderer = m_renderer;
    controlsCommand->menuActor = menuActor;
    controlsCommand->picker->PickFromListOn();

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
        controlsCommand->picker->AddPickList(actor);
    }
    m_pendingActors.clear();

    m_interactor->AddObserver(vtkCommand::Move3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::Select3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::PositionProp3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::Menu3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::Button3DEvent, controlsCommand, 1.0);

    vtkNew<VRTrackpadCommand> trackpadCommand;
    m_interactor->AddObserver(vtkCommand::Elevation3DEvent, trackpadCommand, 1.0);

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
