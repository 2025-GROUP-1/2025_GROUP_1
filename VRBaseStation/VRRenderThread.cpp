
#include "VRRenderThread.h"
#include <QMutexLocker>
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
    // 1. Create all VTK objects ON THIS THREAD.
    m_renderer = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    // This tells VTK to open a desktop window instead of erroring out when no HMD is found
    const char* simulateVR = "1";
    _putenv_s("VTK_VR_SIMULATOR", simulateVR);

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    // 2. Register pending actors.
    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
    }
    m_pendingActors.clear();

    // 3. Lights (Step 5 factors these into LightingHelper).
    vtkNew<vtkLight> keyLight;
    keyLight->SetLightTypeToSceneLight();
    keyLight->SetPosition(3.0, 3.0, 3.0);
    keyLight->SetIntensity(0.9);
    m_renderer->AddLight(keyLight);

    vtkNew<vtkLight> fillLight;
    fillLight->SetLightTypeToSceneLight();
    fillLight->SetPosition(-2.0, 1.5, -2.0);
    fillLight->SetColor(0.7, 0.8, 1.0);
    fillLight->SetIntensity(0.4);
    m_renderer->AddLight(fillLight);

    m_renderer->SetBackground(0.1, 0.1, 0.12);
    m_renderer->ResetCamera(); // Fits all actors into initial view
    m_renderWindow->Initialize();

    if (!m_renderWindow->GetHMD()) {
        qDebug() << "HMD not found! Check SteamVR connection.";
        // Don't return here if simulating, but ensure window exists
    }

    // FIX: Position the camera so you are looking at the models
    vtkCamera* cam = m_renderer->GetActiveCamera();
    cam->SetPosition(0, 1.6, 1.5);   // 1.6m high (eye level), 1.5m back
    cam->SetFocalPoint(0, 1.0, 0);   // Look toward the center of the scene
    cam->SetViewUp(0, 1, 0);         // Ensure Y is "up"

    // 4. Main VR loop.
    m_endRender = false;
    while (!m_endRender) {
        m_renderWindow->Render();
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();
    }

    // 5. Graceful shutdown - release the HMD.
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
            it->second->GetProperty()->SetColor(
                c.redF(), c.greenF(), c.blueF());
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
        // Not needed - parts must be loaded before Start VR.
        break;
    }
}