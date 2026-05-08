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
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <QFileInfo>
#include <QFile>
#include <QStringList>
#include <openvr.h>
#include <unordered_set>
#include <QDebug>
#include <cstdio>

namespace {
using MovementClock = std::chrono::steady_clock;
MovementClock::time_point g_lastDollyMovementTime = MovementClock::time_point::min();
double g_locomotionScaleCompensation = 1.0;

void markDollyMovementHandled()
{
    g_lastDollyMovementTime = MovementClock::now();
}

bool dollyHandledMovementRecently()
{
    return MovementClock::now() - g_lastDollyMovementTime < std::chrono::milliseconds(80);
}

const char* vrDebugPath()
{
    return "C:/work/2025_GROUP_1/vr_debug.txt";
}

void writeVrDebug(const char* mode, const char* fmt, ...)
{
    FILE* f = std::fopen(vrDebugPath(), mode);
    if (!f)
        return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fclose(f);
}
}

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

    // Bypass VTK's start-click state so axis movement reaches Dolly3D directly.
    void OnViewerMovement3D(vtkEventData* edata) override
    {
        this->Dolly3D(edata);
    }

    // HMD-facing movement for any VTK movement events that still reach Dolly3D.
    void Dolly3D(vtkEventData* ed) override
    {
        if (!this->CurrentRenderer)
            return;
        auto* rwi = static_cast<vtkRenderWindowInteractor3D*>(this->Interactor);
        auto* edd = static_cast<vtkEventDataDevice3D*>(ed);
        vtkCamera* cam = this->CurrentRenderer->GetActiveCamera();
        if (!cam)
            return;

        double* trans = rwi->GetPhysicalTranslation(cam);
        if (!trans)
            return;
        if (edd->GetType() == vtkCommand::ViewerMovement3DEvent)
            edd->GetTrackPadPosition(this->LastTrackPadPosition);

        double speedScale = -this->LastTrackPadPosition[1];
        double turnAxis = this->LastTrackPadPosition[0];
        const double turnDeadzone = 0.25;
        const double moveDeadzone = 0.20;
        if (std::abs(turnAxis) < turnDeadzone)
            turnAxis = 0.0;
        if (std::abs(speedScale) < moveDeadzone)
            speedScale = 0.0;
        if (turnAxis == 0.0 && speedScale == 0.0)
            return;

        double physicalScale = rwi->GetPhysicalScale();

        this->LastDolly3DEventTime->StopTimer();
        double dt = std::min(this->LastDolly3DEventTime->GetElapsedTime(), 0.1);
        this->LastDolly3DEventTime->StartTimer();

        double* vd = rwi->GetPhysicalViewDirection();
        if (vd && turnAxis != 0.0) {
            const double yaw = turnAxis * 75.0 * dt;
            const double rad = vtkMath::RadiansFromDegrees(yaw);
            const double c = std::cos(rad);
            const double s = std::sin(rad);
            double nx = c * vd[0] - s * vd[2];
            double nz = s * vd[0] + c * vd[2];
            const double len = std::sqrt(nx * nx + vd[1] * vd[1] + nz * nz);
            if (len > 1e-9)
                rwi->SetPhysicalViewDirection(nx / len, vd[1] / len, nz / len);
        }

        double fwd[3];
        cam->GetDirectionOfProjection(fwd);
        fwd[1] = 0.0;
        if (vtkMath::Norm(fwd) <= 1e-9 && vd) {
            fwd[0] = vd[0];
            fwd[1] = 0.0;
            fwd[2] = vd[2];
        }
        if (vtkMath::Norm(fwd) <= 1e-9) {
            fwd[0] = 0.0;
            fwd[1] = 0.0;
            fwd[2] = -1.0;
        }
        vtkMath::Normalize(fwd);

        double dist = speedScale * this->DollyPhysicalSpeed * physicalScale
            * g_locomotionScaleCompensation * dt;
        rwi->SetPhysicalTranslation(cam,
            trans[0] + fwd[0] * dist,
            trans[1],
            trans[2] + fwd[2] * dist);
        markDollyMovementHandled();

        if (this->AutoAdjustCameraClippingRange)
            this->CurrentRenderer->ResetCameraClippingRange();
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

    // same as update() but takes a precomputed forward vector from OpenVR pose.
    void updateFromForward(const double* pos, const double* fwdIn, vtkRenderer* ren, double maxRay)
    {
        if (!ren) return;

        double fwd[3] = { fwdIn[0], fwdIn[1], fwdIn[2] };
        vtkMath::Normalize(fwd);

        vtkActor* hit = nullptr;
        double hitPt[3] = { 0.0, 0.0, 0.0 };
        try {
            picker->InitializePickList();
            for (vtkActor* actor : pickActors) {
                if (actor) picker->AddPickList(actor);
            }
            picker->PickFromListOn();
            double p[3] = { pos[0], pos[1], pos[2] };
            double o[4] = { 0.0, 0.0, 1.0, 0.0 };
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

    // deterministic controller-ray update from OpenVR tracking poses.
    void tickFromOpenVR(vtkOpenVRRenderWindow* renWin)
    {
        if (!renderer || !renWin) return;
        vr::IVRSystem* hmd = renWin->GetHMD();
        if (!hmd) return;

        auto pickActors = currentPickActors();
        right.setPickActors(pickActors);
        left.setPickActors(pickActors);

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);

        auto updateOne = [&](vr::ETrackedControllerRole role, ControllerRay& ray) {
            vr::TrackedDeviceIndex_t idx = hmd->GetTrackedDeviceIndexForControllerRole(role);
            if (idx == vr::k_unTrackedDeviceIndexInvalid) return;
            if (!poses[idx].bPoseIsValid) return;
            auto& m = poses[idx].mDeviceToAbsoluteTracking;
            double pos[3] = { m.m[0][3], m.m[1][3], m.m[2][3] };
            // Controller forward in OpenVR space is -Z axis of the pose matrix.
            double fwd[3] = { -m.m[0][2], -m.m[1][2], -m.m[2][2] };
            ray.updateFromForward(pos, fwd, renderer, MAX_RAY);
        };

        updateOne(vr::TrackedControllerRole_RightHand, right);
        updateOne(vr::TrackedControllerRole_LeftHand, left);
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
// trigger-based grab (single controller)
// hold trigger on a hovered part to drag it; release to drop
// ---------------------------------------------------------------------------
struct TriggerGrabber {
    bool active = false;
    vtkActor* targetActor = nullptr;
    vr::TrackedDeviceIndex_t activeController = vr::k_unTrackedDeviceIndexInvalid;
    double localOffset[3] = { 0.0, 0.0, 0.0 };

    bool isPartActor(vtkActor* a, std::map<int, vtkSmartPointer<vtkActor>>* actors)
    {
        if (!a || !actors) return false;
        for (auto& [id, act] : *actors)
            if (act.Get() == a) return true;
        return false;
    }

    static bool isTriggerPressed(const vr::VRControllerState_t& state)
    {
        const bool digital = (state.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) != 0;
        const bool analog = state.rAxis[1].x > 0.55f;
        return digital || analog;
    }

    static bool getControllerPos(vr::TrackedDeviceIndex_t idx, vr::TrackedDevicePose_t* poses, double out[3])
    {
        if (idx >= vr::k_unMaxTrackedDeviceCount || !poses[idx].bPoseIsValid)
            return false;
        auto& m = poses[idx].mDeviceToAbsoluteTracking;
        out[0] = m.m[0][3];
        out[1] = m.m[1][3];
        out[2] = m.m[2][3];
        return true;
    }

    void update(vtkOpenVRRenderWindow* renWin, VRRayCallback* rays, std::map<int, vtkSmartPointer<vtkActor>>* activeActors)
    {
        vr::IVRSystem* hmd = renWin ? renWin->GetHMD() : nullptr;
        if (!hmd || !rays || !activeActors) return;

        vr::TrackedDeviceIndex_t leftIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
        vr::TrackedDeviceIndex_t rightIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

        vr::VRControllerState_t lState{}, rState{};
        bool hasLeft = (leftIdx != vr::k_unTrackedDeviceIndexInvalid) &&
                       hmd->GetControllerState(leftIdx, &lState, sizeof(lState));
        bool hasRight = (rightIdx != vr::k_unTrackedDeviceIndexInvalid) &&
                        hmd->GetControllerState(rightIdx, &rState, sizeof(rState));

        const bool leftTrigger = hasLeft && isTriggerPressed(lState);
        const bool rightTrigger = hasRight && isTriggerPressed(rState);

        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);

        if (active) {
            bool stillPressed = false;
            if (activeController == leftIdx) stillPressed = leftTrigger;
            if (activeController == rightIdx) stillPressed = rightTrigger;
            if (!stillPressed || !targetActor) {
                active = false;
                targetActor = nullptr;
                activeController = vr::k_unTrackedDeviceIndexInvalid;
                return;
            }

            double ctrlPos[3];
            if (!getControllerPos(activeController, poses, ctrlPos)) {
                active = false;
                targetActor = nullptr;
                activeController = vr::k_unTrackedDeviceIndexInvalid;
                return;
            }

            targetActor->SetPosition(
                ctrlPos[0] + localOffset[0],
                ctrlPos[1] + localOffset[1],
                ctrlPos[2] + localOffset[2]);
            return;
        }

        if (rightTrigger && rightIdx != vr::k_unTrackedDeviceIndexInvalid &&
            isPartActor(rays->right.hoveredActor, activeActors))
        {
            double ctrlPos[3];
            if (!getControllerPos(rightIdx, poses, ctrlPos)) return;
            targetActor = rays->right.hoveredActor;
            double* p = targetActor->GetPosition();
            localOffset[0] = p[0] - ctrlPos[0];
            localOffset[1] = p[1] - ctrlPos[1];
            localOffset[2] = p[2] - ctrlPos[2];
            activeController = rightIdx;
            active = true;
            return;
        }

        if (leftTrigger && leftIdx != vr::k_unTrackedDeviceIndexInvalid &&
            isPartActor(rays->left.hoveredActor, activeActors))
        {
            double ctrlPos[3];
            if (!getControllerPos(leftIdx, poses, ctrlPos)) return;
            targetActor = rays->left.hoveredActor;
            double* p = targetActor->GetPosition();
            localOffset[0] = p[0] - ctrlPos[0];
            localOffset[1] = p[1] - ctrlPos[1];
            localOffset[2] = p[2] - ctrlPos[2];
            activeController = leftIdx;
            active = true;
        }
    }
};

// ---------------------------------------------------------------------------
// click-free locomotion
// Polls controller axes directly; this avoids VTK's StartMovement action
// getting stuck until some other button/trigger event wakes it up.
// ---------------------------------------------------------------------------
struct PolledLocomotion {
    vtkNew<vtkTimerLog> timer;
    bool timerStarted = false;

    static bool movementAxis(vr::IVRSystem* hmd, vr::ETrackedControllerRole role, double axis[2])
    {
        axis[0] = 0.0;
        axis[1] = 0.0;
        if (!hmd) return false;

        vr::TrackedDeviceIndex_t idx = hmd->GetTrackedDeviceIndexForControllerRole(role);
        if (idx == vr::k_unTrackedDeviceIndexInvalid) return false;

        vr::VRControllerState_t state{};
        if (!hmd->GetControllerState(idx, &state, sizeof(state))) return false;

        axis[0] = state.rAxis[0].x;
        axis[1] = state.rAxis[0].y;
        return true;
    }

    void update(vtkOpenVRRenderWindow* renWin, vtkOpenVRRenderWindowInteractor* interactor,
        vtkRenderer* renderer)
    {
        if (!renWin || !interactor || !renderer) return;
        vtkCamera* cam = renderer->GetActiveCamera();
        if (!cam) return;

        if (!timerStarted) {
            timer->StartTimer();
            timerStarted = true;
            return;
        }

        timer->StopTimer();
        const double dt = std::min(timer->GetElapsedTime(), 0.1);
        timer->StartTimer();

        // If VTK delivered a movement event this frame, Dolly3D already handled it.
        if (dollyHandledMovementRecently())
            return;

        vr::IVRSystem* hmd = renWin->GetHMD();
        if (!hmd) return;

        double left[2], right[2];
        const bool hasLeft = movementAxis(hmd, vr::TrackedControllerRole_LeftHand, left);
        const bool hasRight = movementAxis(hmd, vr::TrackedControllerRole_RightHand, right);

        double axis[2] = { 0.0, 0.0 };
        if (hasLeft) {
            axis[0] += left[0];
            axis[1] += left[1];
        }
        if (hasRight) {
            axis[0] += right[0];
            axis[1] += right[1];
        }

        axis[0] = std::clamp(axis[0], -1.0, 1.0);
        axis[1] = std::clamp(axis[1], -1.0, 1.0);

        const double deadzone = 0.18;
        if (std::abs(axis[0]) < deadzone) axis[0] = 0.0;
        if (std::abs(axis[1]) < deadzone) axis[1] = 0.0;
        if (axis[0] == 0.0 && axis[1] == 0.0) return;

        double* viewDir = interactor->GetPhysicalViewDirection();
        if (viewDir && axis[0] != 0.0) {
            const double yaw = axis[0] * 75.0 * dt;
            const double rad = vtkMath::RadiansFromDegrees(yaw);
            const double c = std::cos(rad);
            const double s = std::sin(rad);
            double nx = c * viewDir[0] - s * viewDir[2];
            double nz = s * viewDir[0] + c * viewDir[2];
            const double len = std::sqrt(nx * nx + viewDir[1] * viewDir[1] + nz * nz);
            if (len > 1e-9) {
                interactor->SetPhysicalViewDirection(nx / len, viewDir[1] / len, nz / len);
            }
        }

        if (axis[1] != 0.0) {
            double fwd[3];
            cam->GetDirectionOfProjection(fwd);
            fwd[1] = 0.0;
            if (vtkMath::Norm(fwd) <= 1e-9 && viewDir) {
                fwd[0] = viewDir[0];
                fwd[1] = 0.0;
                fwd[2] = viewDir[2];
            }
            if (vtkMath::Norm(fwd) <= 1e-9) {
                fwd[0] = 0.0;
                fwd[1] = 0.0;
                fwd[2] = -1.0;
            }
            vtkMath::Normalize(fwd);

            double* trans = interactor->GetPhysicalTranslation(cam);
            if (!trans) return;

            const double dist = -axis[1] * 2.0 * interactor->GetPhysicalScale()
                * g_locomotionScaleCompensation * dt;
            interactor->SetPhysicalTranslation(cam,
                trans[0] + fwd[0] * dist,
                trans[1],
                trans[2] + fwd[2] * dist);
            renderer->ResetCameraClippingRange();
        }
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
    m_hasGarageAnchor = false;
    m_garageCenterX = 0.0;
    m_garageFloorY = 0.0;
    m_garageCenterZ = 0.0;
    m_spawnX = 0.0;
    m_spawnY = 0.0;
    m_spawnZ = 0.0;

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
    bool garageHasBounds = false;
    double garageBounds[6] = { 0, 0, 0, 0, 0, 0 };
    double worldUnitsPerMeter = 1.0;
    if (!glbPath.isEmpty()) {
        vtkNew<vtkGLTFImporter> gltfImporter;
        gltfImporter->SetFileName(glbPath.toStdString().c_str());
        gltfImporter->SetRenderWindow(m_renderWindow);

        vtkActorCollection* actors = m_renderer->GetActors();
        std::unordered_set<vtkActor*> preImportActors;
        actors->InitTraversal();
        vtkActor* a;
        while ((a = actors->GetNextActor()) != nullptr)
            preImportActors.insert(a);

        // import the 3D environment - adds actors, textures, materials, lighting
        gltfImporter->Update();

        // identify only newly imported garage actors (exclude helper actors that already existed)
        std::vector<vtkActor*> envActors;
        actors->InitTraversal();
        while ((a = actors->GetNextActor()) != nullptr) {
            if (preImportActors.find(a) != preImportActors.end())
                continue;
            envActors.push_back(a);
            a->PickableOff();
            a->DragableOff();
        }

        if (envActors.empty()) {
            // Fallback: if actor diffing fails for this GLB, use all non-part actors.
            actors->InitTraversal();
            while ((a = actors->GetNextActor()) != nullptr) {
                bool isPartActor = false;
                for (auto& [id, partAct] : m_activeActors) {
                    if (partAct.Get() == a) { isPartActor = true; break; }
                }
                if (!isPartActor) {
                    envActors.push_back(a);
                    a->PickableOff();
                    a->DragableOff();
                }
            }
        }

        if (!envActors.empty()) {
            bool first = true;
            for (vtkActor* env : envActors) {
                double b[6];
                env->GetBounds(b);
                if (first) {
                    for (int i = 0; i < 6; ++i) garageBounds[i] = b[i];
                    first = false;
                } else {
                    if (b[0] < garageBounds[0]) garageBounds[0] = b[0];
                    if (b[1] > garageBounds[1]) garageBounds[1] = b[1];
                    if (b[2] < garageBounds[2]) garageBounds[2] = b[2];
                    if (b[3] > garageBounds[3]) garageBounds[3] = b[3];
                    if (b[4] < garageBounds[4]) garageBounds[4] = b[4];
                    if (b[5] > garageBounds[5]) garageBounds[5] = b[5];
                }
            }
            garageHasBounds = true;
            const double garageHeight = garageBounds[3] - garageBounds[2];
            // Estimate world-units-per-meter from the garage shell height.
            // Typical interior garage height is around 2.6m.
            if (garageHeight > 1e-6) {
                worldUnitsPerMeter = std::clamp(garageHeight / 2.6, 0.5, 2500.0);
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

    // Calibrate user/world scale. Higher values make the garage feel smaller.
    constexpr double visualScaleBoost = 1.45;
    m_interactor->SetPhysicalScale(worldUnitsPerMeter * visualScaleBoost);
    g_locomotionScaleCompensation = worldUnitsPerMeter / m_interactor->GetPhysicalScale();
    constexpr double viewerHeightOffset = -11.00;

    writeVrDebug("w",
        "[VR init]\n"
        "garageLoaded=%d garageHasBounds=%d actors=%d glb=%s\n"
        "garageBounds=[%f %f] [%f %f] [%f %f]\n"
        "worldUnitsPerMeter=%f physicalScale=%f visualScaleBoost=%f locomotionComp=%f viewerHeightOffset=%f\n",
        (int)garageLoaded, (int)garageHasBounds, (int)m_activeActors.size(),
        glbPath.toStdString().c_str(),
        garageBounds[0], garageBounds[1], garageBounds[2], garageBounds[3],
        garageBounds[4], garageBounds[5],
        worldUnitsPerMeter, m_interactor->GetPhysicalScale(), visualScaleBoost,
        g_locomotionScaleCompensation, viewerHeightOffset);

    if (m_renderer->GetActiveCamera())
        m_renderer->ResetCameraClippingRange();

    // Set user spawn position from garage bounds (works with or without STL)
    if (garageLoaded && garageHasBounds) {
        const double garageCx = (garageBounds[0] + garageBounds[1]) * 0.5;
        const double garageRawFloorY = garageBounds[2];
        const double garageCz = (garageBounds[4] + garageBounds[5]) * 0.5;
        const double garageHeight = garageBounds[3] - garageBounds[2];
        // Some GLB environments include hidden geometry below the visible floor.
        // Lift anchor from raw min-Y to a more usable interior floor estimate.
        const double garageFloorY = garageRawFloorY + std::max(0.0, garageHeight * 0.12);
        m_hasGarageAnchor = true;
        m_garageCenterX = garageCx;
        m_garageFloorY = garageFloorY;
        m_garageCenterZ = garageCz;
        m_spawnX = garageCx;
        // Small world-space offset around the garage floor anchor; keep this decoupled from scale.
        m_spawnY = garageFloorY + viewerHeightOffset;
        m_spawnZ = garageCz;
    } else {
        m_spawnX = 0.0;
        m_spawnY = viewerHeightOffset;
        m_spawnZ = 0.0;
    }

    {
        auto* rwi3d = static_cast<vtkRenderWindowInteractor3D*>(m_interactor.Get());
        double* beforeTrans = rwi3d->GetPhysicalTranslation(m_renderer->GetActiveCamera());
        writeVrDebug("a",
            "[VR spawn before]\n"
            "garageAnchor=%d garageFloorY=%f spawn=(%f,%f,%f) beforeTrans=(%f,%f,%f)\n",
            (int)m_hasGarageAnchor, m_garageFloorY, m_spawnX, m_spawnY, m_spawnZ,
            beforeTrans ? beforeTrans[0] : 0.0,
            beforeTrans ? beforeTrans[1] : 0.0,
            beforeTrans ? beforeTrans[2] : 0.0);
        rwi3d->SetPhysicalTranslation(m_renderer->GetActiveCamera(),
            m_spawnX, m_spawnY, m_spawnZ);
        double* afterTrans = rwi3d->GetPhysicalTranslation(m_renderer->GetActiveCamera());
        writeVrDebug("a",
            "[VR spawn after]\n"
            "afterTrans=(%f,%f,%f)\n",
            afterTrans ? afterTrans[0] : 0.0,
            afterTrans ? afterTrans[1] : 0.0,
            afterTrans ? afterTrans[2] : 0.0);
    }
    m_renderer->ResetCameraClippingRange();

    // Place STL actors in front of user if any are loaded
    if (garageLoaded && garageHasBounds && !m_activeActors.empty()) {
        const double garageWidth = std::max(garageBounds[1] - garageBounds[0], 1e-6);
        const double garageDepth = std::max(garageBounds[5] - garageBounds[4], 1e-6);
        const double garageMinSpan = std::min(garageWidth, garageDepth);
        const double desiredPartSpanMeters = std::clamp((garageMinSpan / m_interactor->GetPhysicalScale()) * 0.16, 0.30, 1.20);
        const double desiredPartSpan = desiredPartSpanMeters * m_interactor->GetPhysicalScale();

        const double partBaseY = m_hasGarageAnchor ? m_garageFloorY : 0.0;
        double targetX = m_spawnX;
        double targetY = partBaseY + 0.55;
        double targetZ = m_spawnZ;
        if (m_renderWindow && m_renderWindow->GetHMD()) {
            vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
                vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
            const auto hmdIdx = vr::k_unTrackedDeviceIndex_Hmd;
            if (poses[hmdIdx].bPoseIsValid) {
                const auto& m = poses[hmdIdx].mDeviceToAbsoluteTracking;
                double fwd[3] = { -m.m[0][2], -m.m[1][2], -m.m[2][2] };
                vtkMath::Normalize(fwd);
                targetX = m.m[0][3] + fwd[0] * 1.0;
                targetY = std::clamp(static_cast<double>(m.m[1][3]), partBaseY + 0.35, partBaseY + 1.10);
                targetZ = m.m[2][3] + fwd[2] * 1.0;
            }
        }
        for (auto& [id, partAct] : m_activeActors) {
            double b[6];
            partAct->GetBounds(b);
            const double partCx = (b[0] + b[1]) * 0.5;
            const double partCy = (b[2] + b[3]) * 0.5;
            const double partCz = (b[4] + b[5]) * 0.5;
            const double partMaxSpan = std::max({ b[1] - b[0], b[3] - b[2], b[5] - b[4], 1e-6 });
            const double stlScale = std::clamp(desiredPartSpan / partMaxSpan, 0.03, 5.0);
            writeVrDebug("a",
                "[VR initial STL]\n"
                "id=%d target=(%f,%f,%f) partBaseY=%f scale=%f bounds=[%f %f] [%f %f] [%f %f]\n",
                id, targetX, targetY, targetZ, partBaseY, stlScale,
                b[0], b[1], b[2], b[3], b[4], b[5]);
            partAct->SetScale(stlScale, stlScale, stlScale);
            partAct->SetPosition(
                targetX - partCx * stlScale,
                targetY - partCy * stlScale,
                targetZ - partCz * stlScale);
            partAct->SetVisibility(1);
            partAct->PickableOn();
        }
        m_renderer->ResetCameraClippingRange();
    }

    m_renderer->ResetCameraClippingRange();

    TriggerGrabber grabber;
    PolledLocomotion locomotion;

    // render loop - process one VR frame, handle commands, repeat
    // NOTE: DoOneEvent already submits frames to both eyes.
    // An extra Render() causes double-submit which results in single-eye flickering.
    m_endRender = false;
    int debugFrameCount = 0;
    while (!m_endRender) {
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        if (debugFrameCount < 12) {
            auto* rwi3d = static_cast<vtkRenderWindowInteractor3D*>(m_interactor.Get());
            double* trans = rwi3d->GetPhysicalTranslation(m_renderer->GetActiveCamera());
            vtkCamera* cam = m_renderer->GetActiveCamera();
            double camPos[3] = { 0.0, 0.0, 0.0 };
            double camFocal[3] = { 0.0, 0.0, 0.0 };
            double camUp[3] = { 0.0, 0.0, 0.0 };
            double clipping[2] = { 0.0, 0.0 };
            if (cam) {
                cam->GetPosition(camPos);
                cam->GetFocalPoint(camFocal);
                cam->GetViewUp(camUp);
                cam->GetClippingRange(clipping);
            }
            double hmdY = 0.0;
            int hmdValid = 0;
            if (m_renderWindow && m_renderWindow->GetHMD()) {
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
                    vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
                const auto hmdIdx = vr::k_unTrackedDeviceIndex_Hmd;
                hmdValid = poses[hmdIdx].bPoseIsValid ? 1 : 0;
                if (hmdValid)
                    hmdY = poses[hmdIdx].mDeviceToAbsoluteTracking.m[1][3];
            }
            writeVrDebug("a",
                "[VR frame %d]\n"
                "physicalScale=%f trans=(%f,%f,%f) hmdValid=%d hmdY=%f\n"
                "cameraPos=(%f,%f,%f) focal=(%f,%f,%f) viewUp=(%f,%f,%f) clip=(%f,%f)\n",
                debugFrameCount, m_interactor->GetPhysicalScale(),
                trans ? trans[0] : 0.0,
                trans ? trans[1] : 0.0,
                trans ? trans[2] : 0.0,
                hmdValid, hmdY,
                camPos[0], camPos[1], camPos[2],
                camFocal[0], camFocal[1], camFocal[2],
                camUp[0], camUp[1], camUp[2],
                clipping[0], clipping[1]);
            ++debugFrameCount;
        }
        locomotion.update(m_renderWindow, m_interactor, m_renderer);
        processCommands();
        rayCallback->tickFromOpenVR(m_renderWindow);
        grabber.update(m_renderWindow, rayCallback, &m_activeActors);
        // Hard-lock environment interactivity every frame so only STL parts can move.
        vtkActorCollection* allActors = m_renderer->GetActors();
        allActors->InitTraversal();
        vtkActor* a = nullptr;
        while ((a = allActors->GetNextActor()) != nullptr) {
            bool isPartActor = false;
            for (auto& [id, partAct] : m_activeActors) {
                if (partAct.Get() == a) { isPartActor = true; break; }
            }
            if (isPartActor) {
                a->PickableOn();
                a->DragableOn();
            } else {
                a->PickableOff();
                a->DragableOff();
            }
        }

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
            cmd.actor->PickableOn();
            cmd.actor->DragableOn();

            double actorBounds[6];
            cmd.actor->GetBounds(actorBounds);
            const double partCx = (actorBounds[0] + actorBounds[1]) * 0.5;
            const double partCy = (actorBounds[2] + actorBounds[3]) * 0.5;
            const double partCz = (actorBounds[4] + actorBounds[5]) * 0.5;
            const double partMaxSpan = std::max({
                actorBounds[1] - actorBounds[0],
                actorBounds[3] - actorBounds[2],
                actorBounds[5] - actorBounds[4],
                1e-6
            });
            double stlScale = 0.10;
            if (m_hasGarageAnchor) {
                // Couple part size to player/world scale.
                const double desiredPartSpan = 0.75 * m_interactor->GetPhysicalScale();
                stlScale = std::clamp(desiredPartSpan / partMaxSpan, 0.03, 5.0);
            }

            const double partBaseY = m_hasGarageAnchor ? m_garageFloorY : 0.0;
            double targetX = m_spawnX;
            double targetY = partBaseY + 0.55;
            double targetZ = m_spawnZ;

            if (m_renderWindow && m_renderWindow->GetHMD()) {
                vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
                vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
                    vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
                const auto hmdIdx = vr::k_unTrackedDeviceIndex_Hmd;
                if (poses[hmdIdx].bPoseIsValid) {
                    const auto& mat = poses[hmdIdx].mDeviceToAbsoluteTracking;
                    double fwd[3] = { -mat.m[0][2], -mat.m[1][2], -mat.m[2][2] };
                    vtkMath::Normalize(fwd);
                    targetX = mat.m[0][3] + fwd[0] * 1.0;
                    targetY = std::clamp(static_cast<double>(mat.m[1][3]), partBaseY + 0.35, partBaseY + 1.10);
                    targetZ = mat.m[2][3] + fwd[2] * 1.0;
                }
            }

            qDebug() << "[VR AddActor] part" << cmd.partID
                     << "bounds:" << actorBounds[0] << actorBounds[1]
                     << actorBounds[2] << actorBounds[3]
                     << actorBounds[4] << actorBounds[5]
                     << "scale:" << stlScale
                     << "target:" << targetX << targetY << targetZ;
            writeVrDebug("a",
                "[VR AddActor]\n"
                "id=%d target=(%f,%f,%f) partBaseY=%f scale=%f bounds=[%f %f] [%f %f] [%f %f]\n",
                cmd.partID, targetX, targetY, targetZ, partBaseY, stlScale,
                actorBounds[0], actorBounds[1], actorBounds[2], actorBounds[3],
                actorBounds[4], actorBounds[5]);

            cmd.actor->SetUserMatrix(nullptr);
            cmd.actor->SetUserTransform(nullptr);
            cmd.actor->SetOrientation(0.0, 0.0, 0.0);
            cmd.actor->SetScale(stlScale, stlScale, stlScale);
            cmd.actor->SetPosition(
                targetX - partCx * stlScale,
                targetY - partCy * stlScale,
                targetZ - partCz * stlScale);
            cmd.actor->SetVisibility(1);

            double finalB[6];
            cmd.actor->GetBounds(finalB);
            qDebug() << "[VR AddActor] final bounds:" << finalB[0] << finalB[1]
                     << finalB[2] << finalB[3] << finalB[4] << finalB[5];

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
