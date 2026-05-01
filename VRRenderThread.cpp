#include "VRRenderThread.h"
#include <QMutexLocker>
#include <QCoreApplication>
#include <vtkActor.h>
#include <vtkOpenVRRenderer.h>
#include <vtkOpenVRRenderWindow.h>
#include <vtkOpenVRRenderWindowInteractor.h>
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
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkCellPicker.h>
#include <vtkOutlineFilter.h>
#include <vtkCommand.h>
#include <vtkEventData.h>
#include <vtkTransform.h>
#include <vtkOpenVRInteractorStyle.h>

// ---------------------------------------------------------------------------
// Per-frame callback: dynamic ray + outline highlight on hovered actor
// ---------------------------------------------------------------------------
// Per-controller state for ray + outline
struct ControllerRay {
    vtkNew<vtkCellPicker>      picker;
    vtkNew<vtkLineSource>      rayLine;
    vtkNew<vtkActor>           rayActor;
    vtkNew<vtkOutlineFilter>   outlineFilter;
    vtkNew<vtkPolyDataMapper>  outlineMapper;
    vtkNew<vtkActor>           outlineActor;
    vtkActor*                  hoveredActor = nullptr;

    void init(vtkRenderer* ren, double r, double g, double b)
    {
        picker->SetTolerance(0.005);

        rayLine->SetPoint1(0, 0, 0);
        rayLine->SetPoint2(0, 0, -1);
        vtkNew<vtkPolyDataMapper> rayMapper;
        rayMapper->SetInputConnection(rayLine->GetOutputPort());
        rayActor->SetMapper(rayMapper);
        rayActor->GetProperty()->SetColor(r, g, b);
        rayActor->GetProperty()->SetLineWidth(2.0);
        rayActor->GetProperty()->SetOpacity(0.7);
        rayActor->PickableOff();
        ren->AddActor(rayActor);

        outlineMapper->SetInputConnection(outlineFilter->GetOutputPort());
        outlineActor->SetMapper(outlineMapper);
        outlineActor->GetProperty()->SetColor(0.1, 1.0, 0.4);
        outlineActor->GetProperty()->SetLineWidth(3.0);
        outlineActor->PickableOff();
        outlineActor->SetVisibility(0);
        ren->AddActor(outlineActor);
    }

    void update(const double* pos, const double* ori, vtkRenderer* ren, double maxRay)
    {
        if (!ren) return;

        vtkNew<vtkTransform> xform;
        xform->RotateWXYZ(ori[0], ori[1], ori[2], ori[3]);
        double fwd[3] = { 0.0, 0.0, -1.0 };
        xform->TransformVector(fwd, fwd);

        double p[3] = { pos[0], pos[1], pos[2] };
        double o[4] = { ori[0], ori[1], ori[2], ori[3] };

        vtkActor* hit = nullptr;
        double hitPt[3] = { 0.0, 0.0, 0.0 };
        try {
            picker->Pick3DRay(p, o, ren);
            hit = vtkActor::SafeDownCast(picker->GetProp3D());
            double* pt = picker->GetPickPosition();
            hitPt[0] = pt[0]; hitPt[1] = pt[1]; hitPt[2] = pt[2];
        } catch (...) {
            hit = nullptr;
        }

        rayLine->SetPoint1(pos[0], pos[1], pos[2]);
        if (hit && picker->GetCellId() >= 0) {
            rayLine->SetPoint2(hitPt[0], hitPt[1], hitPt[2]);
        } else {
            rayLine->SetPoint2(
                pos[0] + fwd[0] * maxRay,
                pos[1] + fwd[1] * maxRay,
                pos[2] + fwd[2] * maxRay);
        }
        rayLine->Modified();

        if (hit != hoveredActor) {
            if (hit && hit->GetMapper() && hit->GetMapper()->GetInput()) {
                outlineFilter->SetInputData(hit->GetMapper()->GetInput());
                outlineFilter->Update();
                outlineActor->SetPosition(hit->GetPosition());
                outlineActor->SetScale(hit->GetScale());
                outlineActor->SetOrientation(hit->GetOrientation());
                outlineActor->SetVisibility(1);
            } else {
                outlineActor->SetVisibility(0);
            }
            hoveredActor = hit;
        }

        if (hoveredActor && outlineActor->GetVisibility()) {
            outlineActor->SetPosition(hoveredActor->GetPosition());
            outlineActor->SetScale(hoveredActor->GetScale());
            outlineActor->SetOrientation(hoveredActor->GetOrientation());
        }
    }
};

class VRRayCallback : public vtkCommand {
public:
    static VRRayCallback* New() { return new VRRayCallback; }

    vtkRenderer* renderer = nullptr;
    ControllerRay right;
    ControllerRay left;

    void init(vtkRenderer* ren)
    {
        renderer = ren;
        right.init(ren, 0.2, 0.85, 1.0);   // cyan
        left.init(ren,  1.0, 0.6,  0.2);    // orange
    }

    void Execute(vtkObject*, unsigned long eventId, void* callData) override
    {
        if (eventId != vtkCommand::Move3DEvent || !renderer)
            return;

        auto* ed = static_cast<vtkEventData*>(callData);
        auto* d3d = ed ? ed->GetAsEventDataDevice3D() : nullptr;
        if (!d3d) return;

        auto device = d3d->GetDevice();
        const double* pos = d3d->GetWorldPosition();
        const double* ori = d3d->GetWorldOrientation();

        if (device == vtkEventDataDevice::RightController ||
            device == vtkEventDataDevice::Unknown ||
            device == vtkEventDataDevice::Any)
        {
            right.update(pos, ori, renderer, MAX_RAY);
        }

        if (device == vtkEventDataDevice::LeftController ||
            device == vtkEventDataDevice::Unknown ||
            device == vtkEventDataDevice::Any)
        {
            left.update(pos, ori, renderer, MAX_RAY);
        }
    }

private:
    static constexpr double MAX_RAY = 10.0;
};

// ---------------------------------------------------------------------------
// VRRenderThread
// ---------------------------------------------------------------------------

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
    m_renderer      = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow  = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor    = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    const std::string manifestPath =
        (QCoreApplication::applicationDirPath() + "/vrbindings/vtk_openvr_actions.json").toStdString();
    m_interactor->SetActionManifestFileName(manifestPath.c_str());
    m_interactor->SetActionSetName("/actions/vtk");

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
    }
    m_pendingActors.clear();

    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // Dynamic ray + outline highlight
    vtkNew<VRRayCallback> rayCallback;
    rayCallback->init(m_renderer);
    m_interactor->AddObserver(vtkCommand::Move3DEvent, rayCallback, 1.0);

    // Skybox
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

        m_skybox = skybox;
    }

    m_renderWindow->Initialize();

    try {
        m_interactor->Initialize();

        // Hide the default red VTK rays — we draw our own
        auto* style = vtkOpenVRInteractorStyle::SafeDownCast(
            m_interactor->GetInteractorStyle());
        if (style) {
            style->HideRay(vtkEventDataDevice::RightController);
            style->HideRay(vtkEventDataDevice::LeftController);
        }
    } catch (...) {
        // VR input system may not be available — continue without custom ray hiding
    }

    if (m_renderer->GetActiveCamera())
        m_renderer->GetActiveCamera()->SetClippingRange(0.001, 100.0);

    m_endRender = false;
    while (!m_endRender) {
        m_renderWindow->Render();
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();
    }

    m_renderWindow->Finalize();
    m_renderer      = nullptr;
    m_renderWindow  = nullptr;
    m_interactor    = nullptr;
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
    case Command::ToggleSkybox: {
        if (m_skybox && m_renderer) {
            if (cmd.data.toBool())
                m_renderer->AddActor(m_skybox);
            else
                m_renderer->RemoveActor(m_skybox);
            m_renderer->ResetCameraClippingRange();
        }
        break;
    }
    }
}
