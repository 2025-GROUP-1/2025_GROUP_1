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
#include <vtkPlaneSource.h>

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
// In-VR editing menu (clip, shrink, colour) triggered by menu button
// ---------------------------------------------------------------------------
struct VRMenu {
    enum Action { Clip, Shrink, ColRed, ColGreen, ColBlue, ColYellow, ColWhite, NUM_ACTIONS };

    struct Button {
        vtkSmartPointer<vtkActor> actor;
        Action action;
    };

    std::vector<Button> buttons;
    bool visible = false;
    int targetPartID = -1;
    vtkRenderer* ren = nullptr;

    static constexpr double BTN_W = 0.06;
    static constexpr double BTN_H = 0.03;
    static constexpr double GAP   = 0.008;

    void create(vtkRenderer* r)
    {
        ren = r;
        addButton(Clip,       1.0, 0.5, 0.0);
        addButton(Shrink,     0.6, 0.2, 0.8);
        addButton(ColRed,     1.0, 0.15, 0.15);
        addButton(ColGreen,   0.15, 0.8, 0.15);
        addButton(ColBlue,    0.15, 0.4, 1.0);
        addButton(ColYellow,  1.0, 1.0, 0.2);
        addButton(ColWhite,   0.9, 0.9, 0.9);
    }

    void addButton(Action a, double cr, double cg, double cb)
    {
        vtkNew<vtkPlaneSource> plane;
        plane->SetOrigin(0, 0, 0);
        plane->SetPoint1(BTN_W, 0, 0);
        plane->SetPoint2(0, BTN_H, 0);
        plane->Update();

        vtkNew<vtkPolyDataMapper> m;
        m->SetInputConnection(plane->GetOutputPort());

        vtkSmartPointer<vtkActor> act = vtkSmartPointer<vtkActor>::New();
        act->SetMapper(m);
        act->GetProperty()->SetColor(cr, cg, cb);
        act->GetProperty()->SetAmbient(1.0);
        act->GetProperty()->SetDiffuse(0.0);
        act->SetVisibility(0);
        act->PickableOn();
        ren->AddActor(act);

        buttons.push_back({act, a});
    }

    void show(double x, double y, double z, int partID)
    {
        visible = true;
        targetPartID = partID;

        double startX = x - (2 * BTN_W + GAP) * 0.5;
        double startY = y + 0.04;

        for (int i = 0; i < 2; i++) {
            buttons[i].actor->SetPosition(
                startX + i * (BTN_W + GAP), startY, z);
            buttons[i].actor->SetVisibility(1);
        }
        double row2Y = startY - BTN_H - GAP;
        int nCol = static_cast<int>(buttons.size()) - 2;
        double row2StartX = x - (nCol * BTN_W + (nCol - 1) * GAP) * 0.5;
        for (int i = 2; i < static_cast<int>(buttons.size()); i++) {
            buttons[i].actor->SetPosition(
                row2StartX + (i - 2) * (BTN_W + GAP), row2Y, z);
            buttons[i].actor->SetVisibility(1);
        }
    }

    void hide()
    {
        visible = false;
        targetPartID = -1;
        for (auto& b : buttons)
            b.actor->SetVisibility(0);
    }

    int findButton(vtkActor* a) const
    {
        for (int i = 0; i < static_cast<int>(buttons.size()); i++)
            if (buttons[i].actor.Get() == a)
                return i;
        return -1;
    }

    bool isMenuActor(vtkActor* a) const { return findButton(a) >= 0; }
};

class VRMenuCallback : public vtkCommand {
public:
    static VRMenuCallback* New() { return new VRMenuCallback; }

    VRRayCallback* rays = nullptr;
    VRMenu menu;
    std::map<int, vtkSmartPointer<vtkActor>>* activeActors = nullptr;
    ModelPartList* partList = nullptr;

    void Execute(vtkObject*, unsigned long, void*) override
    {
        vtkActor* hovered = nullptr;
        if (rays->right.hoveredActor)
            hovered = rays->right.hoveredActor;
        else if (rays->left.hoveredActor)
            hovered = rays->left.hoveredActor;

        int btnIdx = menu.findButton(hovered);
        if (btnIdx >= 0) {
            executeAction(menu.buttons[btnIdx]);
            return;
        }

        if (hovered) {
            int pid = findPartID(hovered);
            if (pid >= 0) {
                if (menu.visible && menu.targetPartID == pid) {
                    menu.hide();
                } else {
                    double* center = hovered->GetCenter();
                    menu.show(center[0], center[1], center[2], pid);
                }
                return;
            }
        }

        if (menu.visible)
            menu.hide();
    }

private:
    int findPartID(vtkActor* a) const
    {
        if (!activeActors) return -1;
        for (auto& [id, act] : *activeActors)
            if (act.Get() == a) return id;
        return -1;
    }

    void executeAction(const VRMenu::Button& btn)
    {
        if (!partList || menu.targetPartID < 0) return;
        ModelPart* part = partList->findByID(menu.targetPartID);
        if (!part) return;

        switch (btn.action) {
        case VRMenu::Clip:
            part->setClipFilter(!part->getClipEnabled());
            part->rebuildVRPipeline();
            break;
        case VRMenu::Shrink:
            part->setShrinkFilter(!part->getShrinkEnabled());
            part->rebuildVRPipeline();
            break;
        case VRMenu::ColRed:
            applyColour(part, QColor(255, 50, 50));
            break;
        case VRMenu::ColGreen:
            applyColour(part, QColor(50, 200, 50));
            break;
        case VRMenu::ColBlue:
            applyColour(part, QColor(50, 100, 255));
            break;
        case VRMenu::ColYellow:
            applyColour(part, QColor(255, 255, 50));
            break;
        case VRMenu::ColWhite:
            applyColour(part, QColor(240, 240, 240));
            break;
        default:
            break;
        }
    }

    void applyColour(ModelPart* part, const QColor& c)
    {
        part->setColour(c);
        vtkActor* vr = part->getVRActor();
        if (vr)
            vr->GetProperty()->SetColor(c.redF(), c.greenF(), c.blueF());
    }
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

    // In-VR editing menu (clip / shrink / colour)
    vtkNew<VRMenuCallback> menuCallback;
    menuCallback->rays = rayCallback;
    menuCallback->activeActors = &m_activeActors;
    menuCallback->partList = m_partList;
    menuCallback->menu.create(m_renderer);
    m_interactor->AddObserver(vtkCommand::Menu3DEvent, menuCallback, 2.0);

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
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);

        // Suppress default VTK red rays every frame (VTK re-shows them during interactions)
        try {
            auto* style = vtkOpenVRInteractorStyle::SafeDownCast(
                m_interactor->GetInteractorStyle());
            if (style) {
                style->HideRay(vtkEventDataDevice::RightController);
                style->HideRay(vtkEventDataDevice::LeftController);
            }
        } catch (...) {}

        processCommands();
        m_renderWindow->Render();
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
