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
#include <vtkMatrix4x4.h>
#include <vtkMath.h>

#include <chrono>
#include <cmath>

static vtkProp3D* g_hoveredActor = nullptr;

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

static void buildControllerMatrix(
    const double* worldPosition,
    const double* worldOrientation,
    vtkMatrix4x4* matrix)
{
    matrix->Identity();

    double rotation[3][3];
    bool hasQuaternion =
        std::fabs(worldOrientation[0]) +
        std::fabs(worldOrientation[1]) +
        std::fabs(worldOrientation[2]) +
        std::fabs(worldOrientation[3]) > 0.0001;

    if (hasQuaternion) {
        vtkMath::QuaternionToMatrix3x3(worldOrientation, rotation);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                matrix->SetElement(row, col, rotation[row][col]);
    }

    matrix->SetElement(0, 3, worldPosition[0]);
    matrix->SetElement(1, 3, worldPosition[1]);
    matrix->SetElement(2, 3, worldPosition[2]);
}

class VRDragCommand : public vtkCommand {
public:
    static VRDragCommand* New() { return new VRDragCommand; }

    vtkRenderer* renderer = nullptr;
    vtkProp3D* lastHoveredActor = nullptr;
    vtkNew<vtkCellPicker> picker;
    vtkProp3D* grabbedActor = nullptr;
    vtkSmartPointer<vtkMatrix4x4> dragMatrix;
    vtkNew<vtkMatrix4x4> controllerStartMatrix;
    vtkNew<vtkMatrix4x4> controllerStartInverseMatrix;
    vtkNew<vtkMatrix4x4> actorStartMatrix;
    std::chrono::steady_clock::time_point lastPickTime = std::chrono::steady_clock::now();
    double lastPickPosition[3] = { 0.0, 0.0, 0.0 };
    double lastPickOrientation[4] = { 0.0, 0.0, 0.0, 0.0 };
    bool hasLastPickPose = false;
    bool triggerDown = false;

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        Q_UNUSED(caller);

        vtkEventData* eventData = static_cast<vtkEventData*>(callData);
        vtkEventDataDevice3D* deviceData = eventData ? eventData->GetAsEventDataDevice3D() : nullptr;

        if (!deviceData || !renderer || !isRightControllerEvent(deviceData->GetDevice()))
            return;

        const double* worldPosition = deviceData->GetWorldPosition();
        const double* worldOrientation = deviceData->GetWorldOrientation();

        if (eventId == vtkCommand::Move3DEvent) {
            updateGrabbedActor(worldPosition, worldOrientation);
            updateHover(worldPosition, worldOrientation);
            return;
        }

        if (eventId == vtkCommand::Select3DEvent) {
            handleSelect(deviceData);
            return;
        }

        if (eventId == vtkCommand::Button3DEvent) {
            if (deviceData->GetInput() == vtkEventDataDeviceInput::Trigger)
                handleSelect(deviceData);
            return;
        }
    }

private:
    void updateGrabbedActor(const double* worldPosition, const double* worldOrientation)
    {
        if (!grabbedActor)
            return;

        vtkNew<vtkMatrix4x4> currentControllerMatrix;
        vtkNew<vtkMatrix4x4> controllerDeltaMatrix;

        buildControllerMatrix(worldPosition, worldOrientation, currentControllerMatrix);
        vtkMatrix4x4::Multiply4x4(
            currentControllerMatrix,
            controllerStartInverseMatrix,
            controllerDeltaMatrix);
        vtkMatrix4x4::Multiply4x4(
            controllerDeltaMatrix,
            actorStartMatrix,
            dragMatrix);
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

        picker->SetTolerance(0.01);
        picker->PickClippingPlanesOff();
        picker->Pick3DRay(pos, ori, renderer);
        vtkProp3D* targetActor = picker->GetProp3D();

        if (targetActor == lastHoveredActor)
            return;

        if (lastHoveredActor && lastHoveredActor != grabbedActor)
            setActorGlow(lastHoveredActor, 0.0, 1.0, 1.0, 1.0);

        if (targetActor && targetActor != grabbedActor)
            setActorGlow(targetActor, 0.45, 0.1, 0.45, 1.0);

        lastHoveredActor = targetActor;
        g_hoveredActor = targetActor;
    }

    void handleSelect(vtkEventDataDevice3D* deviceData)
    {
        AbortFlagOn();

        if (isReleaseAction(deviceData->GetAction())) {
            triggerDown = false;
            releaseGrabbedActor();
            return;
        }

        if (triggerDown)
            return;

        triggerDown = true;

        if (!g_hoveredActor)
            return;

        grabbedActor = g_hoveredActor;
        setActorGlow(grabbedActor, 0.8, 1.0, 0.65, 0.1);

        const double* worldPosition = deviceData->GetWorldPosition();
        const double* worldOrientation = deviceData->GetWorldOrientation();
        buildControllerMatrix(worldPosition, worldOrientation, controllerStartMatrix);
        vtkMatrix4x4::Invert(controllerStartMatrix, controllerStartInverseMatrix);

        actorStartMatrix->DeepCopy(grabbedActor->GetMatrix());
        dragMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        dragMatrix->DeepCopy(actorStartMatrix);
        grabbedActor->SetPosition(0.0, 0.0, 0.0);
        grabbedActor->SetOrientation(0.0, 0.0, 0.0);
        grabbedActor->SetScale(1.0, 1.0, 1.0);
        grabbedActor->SetUserMatrix(dragMatrix);
    }

    void releaseGrabbedActor()
    {
        if (grabbedActor)
            setActorGlow(grabbedActor, 0.45, 0.1, 0.45, 1.0);
        grabbedActor = nullptr;
        dragMatrix = nullptr;
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

    m_renderer->SetAllocatedRenderTime(0.01);

    vtkNew<VRDragCommand> controlsCommand;
    controlsCommand->renderer = m_renderer;
    controlsCommand->picker->PickFromListOn();

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
        controlsCommand->picker->AddPickList(actor);
    }
    m_pendingActors.clear();

    m_interactor->AddObserver(vtkCommand::Move3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::Select3DEvent, controlsCommand, 1.0);
    m_interactor->AddObserver(vtkCommand::Button3DEvent, controlsCommand, 1.0);

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
