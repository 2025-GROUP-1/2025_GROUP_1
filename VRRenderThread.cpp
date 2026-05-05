// VR render thread implementation - OpenVR setup, custom interactor, ray casting, in-VR menu

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
#include <vtkImageReader2.h>
#include <vtkJPEGReader.h>
#include <vtkPNGReader.h>
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
#include <vtkObjectFactory.h>
#include <vtkInteractorStyle3D.h>
#include <vtkTimerLog.h>
#include <vtkRenderWindowInteractor3D.h>
#include <vtkQuaternion.h>
#include <vtkMatrix3x3.h>
#include <vtkMath.h>
#include <vtkGLTFImporter.h>
#include <vtkActorCollection.h>
#include <algorithm>
#include <QFileInfo>
#include <QFile>
#include <QStringList>
#include <openvr.h>

// ---------------------------------------------------------------------------
// custom VR interactor style
// - blocks VTK's built-in probe/clip/exit menu
// - fires a UserEvent instead so our menu callback gets it
// - kills VTK's default red rays (we draw our own)
// - inverts dolly direction so trackpad-up = push away
// ---------------------------------------------------------------------------
class VRCustomStyle : public vtkOpenVRInteractorStyle {
public:
    static VRCustomStyle* New();
    vtkTypeMacro(VRCustomStyle, vtkOpenVRInteractorStyle);

    // replaces VTK's built-in menu with our custom event
    void OnMenu3D(vtkEventData* edata) override
    {
        if (this->Interactor)
            this->Interactor->InvokeEvent(vtkCommand::UserEvent, edata);
    }

    // no-op so VTK never draws its red rays
    void UpdateRay(vtkEventDataDevice) override {}

    // overrides dolly to negate Y direction (trackpad-up = away, down = towards)
    void Dolly3D(vtkEventData* ed) override
    {
        if (!this->CurrentRenderer)
            return;
        auto* rwi = static_cast<vtkRenderWindowInteractor3D*>(this->Interactor);
        auto* edd = static_cast<vtkEventDataDevice3D*>(ed);
        const double* wori = edd->GetWorldOrientation();

        // convert controller orientation to a forward vector
        vtkQuaternion<double> q1;
        q1.SetRotationAngleAndAxis(vtkMath::RadiansFromDegrees(wori[0]), wori[1], wori[2], wori[3]);
        double elem[3][3];
        q1.ToMatrix3x3(elem);
        double vdir[3] = { 0.0, 0.0, -1.0 };
        vtkMatrix3x3::MultiplyPoint(elem[0], vdir, vdir);

        double* trans = rwi->GetPhysicalTranslation(this->CurrentRenderer->GetActiveCamera());
        if (edd->GetType() == vtkCommand::ViewerMovement3DEvent)
            edd->GetTrackPadPosition(this->LastTrackPadPosition);

        // negate so up = push away, down = pull closer
        double speedScale = -this->LastTrackPadPosition[1];
        double physicalScale = rwi->GetPhysicalScale();

        this->LastDolly3DEventTime->StopTimer();
        double dist = speedScale * this->DollyPhysicalSpeed * physicalScale
            * this->LastDolly3DEventTime->GetElapsedTime();
        this->LastDolly3DEventTime->StartTimer();

        rwi->SetPhysicalTranslation(this->CurrentRenderer->GetActiveCamera(),
            trans[0] - vdir[0] * dist,
            trans[1] - vdir[1] * dist,
            trans[2] - vdir[2] * dist);

        if (this->AutoAdjustCameraClippingRange)
            this->CurrentRenderer->ResetCameraClippingRange();
    }

    // -----------------------------------------------------------------------
    // Dual-stick locomotion - polls OpenVR joysticks directly so it works
    // even when the VTK action manifest fails to load (legacy input mode).
    //
    // Left stick:  walk (forward/back + strafe left/right) on flat ground
    // Right stick X: snap turn (45-degree increments)
    // Right stick Y: dolly (push/pull scene)
    //
    // Movement is FLAT (horizontal only) and clamped to floor level.
    // Works on Quest Touch controllers AND Vive Pro wands (trackpad).
    // -----------------------------------------------------------------------
    double m_floorY = 0.0;
    bool m_snapTurnReady = true;   // prevents repeated snap turns while held

    void setFloorY(double y) { m_floorY = y; }

    void pollJoystickDolly(vtkOpenVRRenderWindow* renWin)
    {
        if (!this->CurrentRenderer || !renWin)
            return;

        vr::IVRSystem* hmd = renWin->GetHMD();
        if (!hmd) return;

        auto* rwi = static_cast<vtkRenderWindowInteractor3D*>(this->Interactor);
        if (!rwi) return;

        vtkCamera* cam = this->CurrentRenderer->GetActiveCamera();
        if (!cam) return;

        double physicalScale = rwi->GetPhysicalScale();
        static constexpr float DEADZONE = 0.2f;
        double moveSpeed = physicalScale * 0.015;

        // use the play-space forward direction (not head tilt) for horizontal movement
        double* physDir = renWin->GetPhysicalViewDirection();
        // project physical view direction onto horizontal plane (Y=0)
        double fwd[3] = { physDir[0], 0.0, physDir[2] };
        double fLen = sqrt(fwd[0]*fwd[0] + fwd[2]*fwd[2]);
        if (fLen > 1e-6) { fwd[0] /= fLen; fwd[2] /= fLen; }
        // right vector perpendicular to forward on the ground
        double right[3] = { fwd[2], 0.0, -fwd[0] };

        // --- LEFT STICK: walk (flat ground only) ---
        vr::TrackedDeviceIndex_t leftIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);

        if (leftIdx != vr::k_unTrackedDeviceIndexInvalid) {
            vr::VRControllerState_t lState;
            if (hmd->GetControllerState(leftIdx, &lState, sizeof(lState))) {
                float lx = lState.rAxis[0].x;
                float ly = lState.rAxis[0].y;

                if (std::abs(lx) <= DEADZONE) lx = 0.0f;
                if (std::abs(ly) <= DEADZONE) ly = 0.0f;

                if (lx != 0.0f || ly != 0.0f) {
                    double dx = (fwd[0] * ly + right[0] * lx) * moveSpeed;
                    double dz = (fwd[2] * ly + right[2] * lx) * moveSpeed;

                    double* trans = renWin->GetPhysicalTranslation();
                    double newX = trans[0] + dx;
                    double newZ = trans[2] + dz;

                    // clamp to floor - don't allow going below floor level
                    renWin->SetPhysicalTranslation(newX, m_floorY, newZ);
                }
            }
        }

        // --- RIGHT STICK: snap turn (X) + dolly (Y) ---
        vr::TrackedDeviceIndex_t rightIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

        if (rightIdx != vr::k_unTrackedDeviceIndexInvalid) {
            vr::VRControllerState_t rState;
            if (hmd->GetControllerState(rightIdx, &rState, sizeof(rState))) {
                float rx = rState.rAxis[0].x;
                float ry = rState.rAxis[0].y;

                // snap turn: 45 degrees per flick
                // rotates the physical view direction AND the translation
                // so the user stays in place but the world rotates around them
                if (std::abs(rx) > 0.7f) {
                    if (m_snapTurnReady) {
                        double angle = (rx > 0) ? -45.0 : 45.0;
                        double rad = angle * vtkMath::Pi() / 180.0;
                        double cosA = cos(rad);
                        double sinA = sin(rad);

                        // rotate physical view direction around Y
                        double newDir[3] = {
                            physDir[0] * cosA - physDir[2] * sinA,
                            physDir[1],
                            physDir[0] * sinA + physDir[2] * cosA
                        };
                        renWin->SetPhysicalViewDirection(newDir[0], newDir[1], newDir[2]);

                        // also rotate the physical view up if it's not pure Y-up
                        double* viewUp = renWin->GetPhysicalViewUp();
                        double newUp[3] = {
                            viewUp[0] * cosA - viewUp[2] * sinA,
                            viewUp[1],
                            viewUp[0] * sinA + viewUp[2] * cosA
                        };
                        renWin->SetPhysicalViewUp(newUp[0], newUp[1], newUp[2]);

                        m_snapTurnReady = false;
                    }
                } else {
                    m_snapTurnReady = true;
                }

                // right stick Y = dolly (push/pull scene along ground)
                if (std::abs(ry) > DEADZONE) {
                    double speed = static_cast<double>(ry) * moveSpeed;
                    double* trans = renWin->GetPhysicalTranslation();
                    renWin->SetPhysicalTranslation(
                        trans[0] + fwd[0] * speed,
                        m_floorY,
                        trans[2] + fwd[2] * speed);
                }
            }
        }
    }
};
vtkStandardNewMacro(VRCustomStyle);

// ---------------------------------------------------------------------------
// controller ray + outline highlight
// each controller gets a coloured ray and an outline box around hovered actors
// ---------------------------------------------------------------------------
struct ControllerRay {
    vtkNew<vtkCellPicker>      picker;
    vtkNew<vtkLineSource>      rayLine;
    vtkNew<vtkActor>           rayActor;
    vtkNew<vtkOutlineFilter>   outlineFilter;
    vtkNew<vtkPolyDataMapper>  outlineMapper;
    vtkNew<vtkActor>           outlineActor;
    vtkActor*                  hoveredActor = nullptr;
    std::vector<vtkActor*>     pickActors;

    // creates the ray line and outline actors, adds them to the renderer
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

    void setPickActors(const std::vector<vtkActor*>& actors)
    {
        pickActors = actors;
    }

    // fires a pick ray from the controller, updates the ray line endpoint and outline
    void update(const double* pos, const double* ori, vtkRenderer* ren, double maxRay)
    {
        if (!ren) return;

        // work out which direction the controller is pointing
        vtkNew<vtkTransform> xform;
        xform->RotateWXYZ(ori[0], ori[1], ori[2], ori[3]);
        double fwd[3] = { 0.0, 0.0, -1.0 };
        xform->TransformVector(fwd, fwd);

        double p[3] = { pos[0], pos[1], pos[2] };
        double o[4] = { ori[0], ori[1], ori[2], ori[3] };

        // try to pick an actor along the ray
        vtkActor* hit = nullptr;
        double hitPt[3] = { 0.0, 0.0, 0.0 };
        try {
            picker->InitializePickList();
            for (vtkActor* actor : pickActors) {
                if (actor)
                    picker->AddPickList(actor);
            }
            picker->PickFromListOn();
            picker->Pick3DRay(p, o, ren);
            hit = vtkActor::SafeDownCast(picker->GetProp3D());
            double* pt = picker->GetPickPosition();
            hitPt[0] = pt[0]; hitPt[1] = pt[1]; hitPt[2] = pt[2];
        } catch (...) {
            hit = nullptr;
        }

        // ray goes from controller to hit point (or extends to max length)
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

        // show an outline box around whatever the ray is pointing at
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

        // keep the outline tracking the actor if it moves
        if (hoveredActor && outlineActor->GetVisibility()) {
            outlineActor->SetPosition(hoveredActor->GetPosition());
            outlineActor->SetScale(hoveredActor->GetScale());
            outlineActor->SetOrientation(hoveredActor->GetOrientation());
        }
    }
};

// handles Move3D events and updates both controller rays each frame
class VRRayCallback : public vtkCommand {
public:
    static VRRayCallback* New() { return new VRRayCallback; }

    vtkRenderer* renderer = nullptr;
    std::map<int, vtkSmartPointer<vtkActor>>* activeActors = nullptr;
    std::vector<vtkActor*> menuActors;
    ControllerRay right;
    ControllerRay left;

    void init(vtkRenderer* ren)
    {
        renderer = ren;
        right.init(ren, 0.2, 0.85, 1.0);   // cyan for right
        left.init(ren,  1.0, 0.6,  0.2);    // orange for left
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
        auto pickActors = currentPickActors();

        if (device == vtkEventDataDevice::RightController ||
            device == vtkEventDataDevice::Unknown ||
            device == vtkEventDataDevice::Any)
        {
            right.setPickActors(pickActors);
            right.update(pos, ori, renderer, MAX_RAY);
        }

        if (device == vtkEventDataDevice::LeftController ||
            device == vtkEventDataDevice::Unknown ||
            device == vtkEventDataDevice::Any)
        {
            left.setPickActors(pickActors);
            left.update(pos, ori, renderer, MAX_RAY);
        }
    }

private:
    static constexpr double MAX_RAY = 10000.0;   // ray length in mm

    std::vector<vtkActor*> currentPickActors() const
    {
        std::vector<vtkActor*> actors;
        if (activeActors) {
            for (const auto& [id, actor] : *activeActors) {
                if (actor)
                    actors.push_back(actor);
            }
        }
        actors.insert(actors.end(), menuActors.begin(), menuActors.end());
        return actors;
    }
};

// ---------------------------------------------------------------------------
// in-VR editing menu
// shows coloured buttons above a hovered part for clip/shrink/colour changes
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

    // button sizes (scaled by physical scale at show time)
    double BTN_W = 60.0;
    double BTN_H = 30.0;
    double GAP   = 8.0;

    // builds all the menu button actors
    void create(vtkRenderer* r)
    {
        ren = r;
        addButton(Clip,       1.0, 0.5, 0.0);    // orange
        addButton(Shrink,     0.6, 0.2, 0.8);    // purple
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

    // positions the menu above the given world position, scaled for VR
    void show(double x, double y, double z, int partID, double physScale = 1.0)
    {
        visible = true;
        targetPartID = partID;

        // scale button sizes based on physical scale so they're always comfortable
        double scale = physScale * 0.05;  // 5% of physical scale gives good button size
        double bw = BTN_W * scale;
        double bh = BTN_H * scale;
        double gap = GAP * scale;

        // resize all button planes
        for (auto& b : buttons) {
            b.actor->SetScale(scale, scale, 1.0);
        }

        // top row: clip + shrink
        double startX = x - (2 * bw + gap) * 0.5;
        double startY = y + 40.0 * scale;

        for (int i = 0; i < 2; i++) {
            buttons[i].actor->SetPosition(
                startX + i * (bw + gap), startY, z);
            buttons[i].actor->SetVisibility(1);
        }

        // bottom row: colour swatches
        double row2Y = startY - bh - gap;
        int nCol = static_cast<int>(buttons.size()) - 2;
        double row2StartX = x - (nCol * bw + (nCol - 1) * gap) * 0.5;
        for (int i = 2; i < static_cast<int>(buttons.size()); i++) {
            buttons[i].actor->SetPosition(
                row2StartX + (i - 2) * (bw + gap), row2Y, z);
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

    std::vector<vtkActor*> actors() const
    {
        std::vector<vtkActor*> result;
        for (const auto& button : buttons)
            result.push_back(button.actor);
        return result;
    }
};

// handles menu button presses - fires when the user hits the menu button on the controller
class VRMenuCallback : public vtkCommand {
public:
    static VRMenuCallback* New() { return new VRMenuCallback; }

    VRRayCallback* rays = nullptr;
    VRMenu menu;
    std::map<int, vtkSmartPointer<vtkActor>>* activeActors = nullptr;
    ModelPartList* partList = nullptr;
    VRRenderThread* vrThread = nullptr;

    vtkOpenVRRenderWindow* renWindow = nullptr;

    void Execute(vtkObject*, unsigned long, void*) override
    {
        // check what the controller is pointing at
        vtkActor* hovered = nullptr;
        if (rays->right.hoveredActor)
            hovered = rays->right.hoveredActor;
        else if (rays->left.hoveredActor)
            hovered = rays->left.hoveredActor;

        // if pointing at a menu button, run its action
        int btnIdx = menu.findButton(hovered);
        if (btnIdx >= 0) {
            executeAction(menu.buttons[btnIdx]);
            return;
        }

        // if pointing at a part, toggle the menu for that part
        if (hovered) {
            int pid = findPartID(hovered);
            if (pid >= 0) {
                if (menu.visible && menu.targetPartID == pid) {
                    menu.hide();
                } else {
                    double* center = hovered->GetCenter();
                    double physScale = 1.0;
                    if (renWindow)
                        physScale = renWindow->GetPhysicalScale();
                    menu.show(center[0], center[1], center[2], pid, physScale);
                }
                return;
            }
        }

        // pointing at nothing - close menu
        if (menu.visible)
            menu.hide();
    }

private:
    // looks up which part ID an actor belongs to
    int findPartID(vtkActor* a) const
    {
        if (!activeActors) return -1;
        for (auto& [id, act] : *activeActors)
            if (act.Get() == a) return id;
        return -1;
    }

    // runs the selected menu action on the target part (VR thread - signals GUI to sync)
    void executeAction(const VRMenu::Button& btn)
    {
        if (!partList || menu.targetPartID < 0) return;
        ModelPart* part = partList->findByID(menu.targetPartID);
        if (!part) return;

        switch (btn.action) {
        case VRMenu::Clip: {
            bool newState = !part->getClipEnabled();
            part->m_clipEnabled = newState;
            part->rebuildVRPipeline();
            if (vrThread) emit vrThread->partClipChanged(menu.targetPartID, newState);
            break;
        }
        case VRMenu::Shrink: {
            bool newState = !part->getShrinkEnabled();
            part->m_shrinkEnabled = newState;
            part->rebuildVRPipeline();
            if (vrThread) emit vrThread->partShrinkChanged(menu.targetPartID, newState);
            break;
        }
        case VRMenu::ColRed:
            applyColourVR(part, QColor(255, 50, 50));
            break;
        case VRMenu::ColGreen:
            applyColourVR(part, QColor(50, 200, 50));
            break;
        case VRMenu::ColBlue:
            applyColourVR(part, QColor(50, 100, 255));
            break;
        case VRMenu::ColYellow:
            applyColourVR(part, QColor(255, 255, 50));
            break;
        case VRMenu::ColWhite:
            applyColourVR(part, QColor(240, 240, 240));
            break;
        default:
            break;
        }
    }

    // updates VR actor and signals GUI to apply the same colour on its side
    void applyColourVR(ModelPart* part, const QColor& c)
    {
        part->m_colour = c;
        vtkActor* vr = part->getVRActor();
        if (vr)
            vr->GetProperty()->SetColor(c.redF(), c.greenF(), c.blueF());
        if (vrThread) emit vrThread->partColourChanged(menu.targetPartID, c);
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

// registers an actor before the thread starts (called from the GUI thread)
void VRRenderThread::addActorOffline(vtkActor* actor, int partID) {
    if (!actor) return;
    m_pendingActors.emplace_back(partID, vtkSmartPointer<vtkActor>(actor));
}

// thread-safe command queue - GUI pushes, VR loop pops
void VRRenderThread::issueCommand(Command c, int partID, const QVariant& data) {
    QMutexLocker locker(&m_mutex);
    m_commandQueue.enqueue({ c, partID, data, nullptr });
}

// overload for commands that need an actor (AddActor, RemoveActor)
void VRRenderThread::issueCommand(Command c, int partID, vtkActor* actor) {
    QMutexLocker locker(&m_mutex);
    m_commandQueue.enqueue({ c, partID, QVariant(), vtkSmartPointer<vtkActor>(actor) });
}

// the main VR loop - sets up OpenVR, adds actors, runs render loop
void VRRenderThread::run() {
    m_renderer      = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow  = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor    = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    // point the interactor at our SteamVR action bindings
    const std::string manifestPath =
        (QCoreApplication::applicationDirPath() + "/vrbindings/vtk_openvr_actions.json").toStdString();
    m_interactor->SetActionManifestFileName(manifestPath.c_str());
    m_interactor->SetActionSetName("/actions/vtk");

    // custom style: blocks VTK's built-in menu, inverts dolly, hides default rays
    vtkNew<VRCustomStyle> customStyle;
    m_interactor->SetInteractorStyle(customStyle);

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    // add all the parts that were queued before the thread started
    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
    }
    m_pendingActors.clear();

    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // set up dual-controller ray casting + outline highlight
    vtkNew<VRRayCallback> rayCallback;
    rayCallback->init(m_renderer);
    rayCallback->activeActors = &m_activeActors;
    m_interactor->AddObserver(vtkCommand::Move3DEvent, rayCallback, 1.0);

    // in-VR editing menu (clip / shrink / colour buttons)
    vtkNew<VRMenuCallback> menuCallback;
    menuCallback->rays = rayCallback;
    menuCallback->activeActors = &m_activeActors;
    menuCallback->partList = m_partList;
    menuCallback->vrThread = this;
    menuCallback->renWindow = m_renderWindow;
    menuCallback->menu.create(m_renderer);
    rayCallback->menuActors = menuCallback->menu.actors();
    m_interactor->AddObserver(vtkCommand::UserEvent, menuCallback, 2.0);

    m_renderWindow->Initialize();

    // load 3D garage environment (garage__warehouse.glb or garage.glb) as actual geometry
    QString glbPath;
    QStringList glbNames = { "garage__warehouse.glb", "garage.glb" };
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath(),
        QCoreApplication::applicationDirPath() + "/..",
        QCoreApplication::applicationDirPath() + "/../..",
        QCoreApplication::applicationDirPath() + "/../../.."
    };
    for (const auto& dir : searchDirs) {
        for (const auto& name : glbNames) {
            QString candidate = dir + "/" + name;
            if (QFile::exists(candidate)) { glbPath = candidate; break; }
        }
        if (!glbPath.isEmpty()) break;
    }

    bool garageLoaded = false;
    if (!glbPath.isEmpty()) {
        vtkNew<vtkGLTFImporter> gltfImporter;
        gltfImporter->SetFileName(glbPath.toStdString().c_str());
        gltfImporter->SetRenderWindow(m_renderWindow);

        // import the 3D environment - adds actors, textures, materials, lighting
        gltfImporter->Update();

        // mark all imported actors as non-pickable (environment only)
        // and collect them so we can position the environment correctly
        std::vector<vtkActor*> envActors;
        vtkActorCollection* actors = m_renderer->GetActors();
        actors->InitTraversal();
        vtkActor* a;
        while ((a = actors->GetNextActor()) != nullptr) {
            bool isPartActor = false;
            for (auto& [id, partAct] : m_activeActors) {
                if (partAct.Get() == a) { isPartActor = true; break; }
            }
            if (!isPartActor) {
                a->PickableOff();
                envActors.push_back(a);
            }
        }

        // position the garage so the user's STL parts sit inside it at a reasonable height
        // GLTF uses metres, VTK OpenVR also uses metres, so typically no scale is needed.
        // If the garage is too large or small relative to the STL, adjust it.
        if (!envActors.empty()) {
            // compute bounds of all environment geometry
            double envBounds[6] = { 1e30, -1e30, 1e30, -1e30, 1e30, -1e30 };
            for (auto* ea : envActors) {
                double b[6];
                ea->GetBounds(b);
                if (b[0] < envBounds[0]) envBounds[0] = b[0];
                if (b[1] > envBounds[1]) envBounds[1] = b[1];
                if (b[2] < envBounds[2]) envBounds[2] = b[2];
                if (b[3] > envBounds[3]) envBounds[3] = b[3];
                if (b[4] < envBounds[4]) envBounds[4] = b[4];
                if (b[5] > envBounds[5]) envBounds[5] = b[5];
            }

            // compute bounds of loaded STL parts
            double partBounds[6] = { 1e30, -1e30, 1e30, -1e30, 1e30, -1e30 };
            bool hasParts = false;
            for (auto& [id, partAct] : m_activeActors) {
                double b[6];
                partAct->GetBounds(b);
                if (b[0] < partBounds[0]) partBounds[0] = b[0];
                if (b[1] > partBounds[1]) partBounds[1] = b[1];
                if (b[2] < partBounds[2]) partBounds[2] = b[2];
                if (b[3] > partBounds[3]) partBounds[3] = b[3];
                if (b[4] < partBounds[4]) partBounds[4] = b[4];
                if (b[5] > partBounds[5]) partBounds[5] = b[5];
                hasParts = true;
            }

            // if the garage is significantly different scale from parts, rescale it
            // so the garage wraps around the parts comfortably
            if (hasParts) {
                double envSize = std::max({ envBounds[1]-envBounds[0],
                                            envBounds[3]-envBounds[2],
                                            envBounds[5]-envBounds[4] });
                double partSize = std::max({ partBounds[1]-partBounds[0],
                                             partBounds[3]-partBounds[2],
                                             partBounds[5]-partBounds[4] });

                // target: garage should be ~5x bigger than the parts
                if (envSize > 1e-6 && partSize > 1e-6) {
                    double targetEnvSize = partSize * 5.0;
                    double scaleFactor = targetEnvSize / envSize;

                    // only rescale if the difference is significant (>2x off)
                    if (scaleFactor < 0.5 || scaleFactor > 2.0) {
                        for (auto* ea : envActors)
                            ea->SetScale(scaleFactor, scaleFactor, scaleFactor);
                    }
                }

                // centre the garage around the parts
                double partCx = (partBounds[0] + partBounds[1]) * 0.5;
                double partCy = partBounds[2];  // floor = bottom of parts
                double partCz = (partBounds[4] + partBounds[5]) * 0.5;

                double envCx = (envBounds[0] + envBounds[1]) * 0.5;
                double envCy = envBounds[2];   // floor of garage
                double envCz = (envBounds[4] + envBounds[5]) * 0.5;

                double offsetX = partCx - envCx;
                double offsetY = partCy - envCy;
                double offsetZ = partCz - envCz;

                for (auto* ea : envActors) {
                    double* pos = ea->GetPosition();
                    ea->SetPosition(pos[0] + offsetX, pos[1] + offsetY, pos[2] + offsetZ);
                }
            }
        }

        garageLoaded = true;
    }

    // fallback: 2D skybox if no 3D garage available
    if (!garageLoaded) {
        QString garagePath2D = QCoreApplication::applicationDirPath() + "/room2.png";
        QString roomPath     = QCoreApplication::applicationDirPath() + "/room.jpg";

        vtkSmartPointer<vtkImageReader2> bgReader;
        vtkNew<vtkPNGReader>  pngReader;
        vtkNew<vtkJPEGReader> jpgReader;

        if (pngReader->CanReadFile(garagePath2D.toStdString().c_str())) {
            pngReader->SetFileName(garagePath2D.toStdString().c_str());
            pngReader->Update();
            bgReader = pngReader;
        } else if (jpgReader->CanReadFile(roomPath.toStdString().c_str())) {
            jpgReader->SetFileName(roomPath.toStdString().c_str());
            jpgReader->Update();
            bgReader = jpgReader;
        }

        if (bgReader) {
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
    }

    try {
        m_interactor->Initialize();
    } catch (...) {
        // VR input system may not be available
    }

    if (m_renderer->GetActiveCamera())
        m_renderer->ResetCameraClippingRange();

    // position user standing on the floor, not inside it
    // shifts VR origin so real-world floor = bottom of scene geometry
    {
        double bounds[6];
        m_renderer->ComputeVisiblePropBounds(bounds);
        double* trans = m_renderWindow->GetPhysicalTranslation();
        double floorY = bounds[2];   // Y-min of all geometry
        m_renderWindow->SetPhysicalTranslation(trans[0], floorY, trans[2]);
        customStyle->setFloorY(floorY);
    }

    // render loop - process one VR frame, handle commands, repeat
    // NOTE: DoOneEvent already submits frames to both eyes.
    // An extra Render() causes double-submit which results in single-eye flickering.
    m_endRender = false;
    while (!m_endRender) {
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();

        // fallback joystick locomotion - polls OpenVR directly so it works
        // even when the action manifest fails to load (legacy input mode on Quest)
        customStyle->pollJoystickDolly(m_renderWindow);
    }

    m_renderWindow->Finalize();
    m_renderer      = nullptr;
    m_renderWindow  = nullptr;
    m_interactor    = nullptr;
}

// drains the command queue (called from the VR thread each frame)
void VRRenderThread::processCommands() {
    QMutexLocker locker(&m_mutex);
    while (!m_commandQueue.isEmpty()) {
        applyCommand(m_commandQueue.dequeue());
    }
}

// executes a single command from the GUI
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
    case Command::AddActor: {
        if (cmd.actor && m_renderer) {
            m_renderer->AddActor(cmd.actor);
            m_activeActors[cmd.partID] = cmd.actor;
            m_renderer->ResetCameraClippingRange();
        }
        break;
    }
    case Command::RemoveActor: {
        auto it = m_activeActors.find(cmd.partID);
        if (it != m_activeActors.end() && m_renderer) {
            m_renderer->RemoveActor(it->second);
            m_activeActors.erase(it);
            m_renderer->ResetCameraClippingRange();
        }
        break;
    }
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
