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

    // Clear manifest to avoid the vr::VRInput() nullptr crash
    m_interactor->SetActionManifestFileName("");
    m_interactor->SetActionSetName("");

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
    }
    m_pendingActors.clear();

    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // Skybox — looks for room.jpg next to the executable
    QString roomPath = QCoreApplication::applicationDirPath() + "/room.jpg";
    vtkNew<vtkJPEGReader> bgReader;
    bgReader->SetFileName(roomPath.toStdString().c_str());
    bgReader->Update();

    if (bgReader->GetOutput()->GetNumberOfPoints() > 0) {
        vtkNew<vtkTexture> bgTexture;
        bgTexture->SetInputConnection(bgReader->GetOutputPort());
        bgTexture->InterpolateOn();

        vtkNew<vtkSkybox> skybox;
        skybox->SetTexture(bgTexture);
        skybox->SetProjectionToSphere();
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
