// VR render thread implementation - OpenVR setup, custom interactor, ray casting, per-actor scale

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
#include <vtkPNGReader.h>
#include <vtkImageReader2.h>
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
#include <vtkObjectFactory.h>
#include <vtkInteractorStyle3D.h>
#include <vtkTimerLog.h>
#include <vtkRenderWindowInteractor3D.h>
#include <vtkQuaternion.h>
#include <vtkMatrix3x3.h>
#include <vtkMath.h>
#include <vtkGLTFImporter.h>
#include <vtkActorCollection.h>
#include <QFile>
#include <openvr.h>
#include <cmath>

// ---------------------------------------------------------------------------
// custom VR interactor style (based on e2776a5 — the version that worked)
// - blocks VTK's built-in menu (fires UserEvent instead)
// - kills VTK's default red rays (we draw our own)
// - inverts dolly direction so trackpad-up = push away
// - blocks world-scale pinch (we do per-actor scale instead)
// VTK's built-in grip grab (pan/rotate via complex gesture) is left working
// ---------------------------------------------------------------------------
class VRCustomStyle : public vtkOpenVRInteractorStyle {
public:
    static VRCustomStyle* New();
    vtkTypeMacro(VRCustomStyle, vtkOpenVRInteractorStyle);

    void OnMenu3D(vtkEventData* edata) override
    {
        if (this->Interactor)
            this->Interactor->InvokeEvent(vtkCommand::UserEvent, edata);
    }

    void UpdateRay(vtkEventDataDevice) override {}

    // prevent world scaling — per-actor scale is handled separately
    void OnPinch() override {}

    // inverted dolly: trackpad/joystick up = push away, down = pull closer
    void Dolly3D(vtkEventData* ed) override
    {
        if (!this->CurrentRenderer)
            return;
        auto* rwi = static_cast<vtkRenderWindowInteractor3D*>(this->Interactor);
        auto* edd = static_cast<vtkEventDataDevice3D*>(ed);
        const double* wori = edd->GetWorldOrientation();

        vtkQuaternion<double> q1;
        q1.SetRotationAngleAndAxis(vtkMath::RadiansFromDegrees(wori[0]), wori[1], wori[2], wori[3]);
        double elem[3][3];
        q1.ToMatrix3x3(elem);
        double vdir[3] = { 0.0, 0.0, -1.0 };
        vtkMatrix3x3::MultiplyPoint(elem[0], vdir, vdir);

        double* trans = rwi->GetPhysicalTranslation(this->CurrentRenderer->GetActiveCamera());
        if (edd->GetType() == vtkCommand::ViewerMovement3DEvent)
            edd->GetTrackPadPosition(this->LastTrackPadPosition);

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

    void init(vtkRenderer* ren, double r, double g, double b)
    {
        picker->SetTolerance(5.0);

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

// handles Move3D events and updates both controller rays each frame
class VRRayCallback : public vtkCommand {
public:
    static VRRayCallback* New() { return new VRRayCallback; }

    vtkRenderer* renderer = nullptr;
    ControllerRay right;
    ControllerRay left;

    void init(vtkRenderer* ren)
    {
        renderer = ren;
        right.init(ren, 0.2, 0.85, 1.0);
        left.init(ren,  1.0, 0.6,  0.2);
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
    static constexpr double MAX_RAY = 10000.0;
};

// ---------------------------------------------------------------------------
// per-actor scale via two-grip pinch
// polls OpenVR for grip state; when both controllers grip while pointing
// at the same part actor, changes that actor's scale based on hand distance
// ---------------------------------------------------------------------------
struct PerActorScaler {
    bool active = false;
    vtkActor* targetActor = nullptr;
    double startDist = 0.0;
    double startScale[3] = {1, 1, 1};

    bool isPartActor(vtkActor* a, std::map<int, vtkSmartPointer<vtkActor>>* actors)
    {
        if (!a || !actors) return false;
        for (auto& [id, act] : *actors)
            if (act.Get() == a) return true;
        return false;
    }

    void update(vtkOpenVRRenderWindow* renWin, VRRayCallback* rays,
                std::map<int, vtkSmartPointer<vtkActor>>* activeActors)
    {
        vr::IVRSystem* hmd = renWin->GetHMD();
        if (!hmd) return;

        vr::TrackedDeviceIndex_t leftIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
        vr::TrackedDeviceIndex_t rightIdx =
            hmd->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);

        vr::VRControllerState_t lState{}, rState{};
        bool hasLeft = (leftIdx != vr::k_unTrackedDeviceIndexInvalid) &&
                       hmd->GetControllerState(leftIdx, &lState, sizeof(lState));
        bool hasRight = (rightIdx != vr::k_unTrackedDeviceIndexInvalid) &&
                        hmd->GetControllerState(rightIdx, &rState, sizeof(rState));

        // detect grip on both Vive (digital) and Oculus Touch (analog axis 2)
        bool leftGrip = hasLeft && (
            (lState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) ||
            lState.rAxis[2].x > 0.5f);
        bool rightGrip = hasRight && (
            (rState.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) ||
            rState.rAxis[2].x > 0.5f);

        bool bothGrip = leftGrip && rightGrip;

        if (!bothGrip) {
            active = false;
            targetActor = nullptr;
            return;
        }

        // both grips are held — find if both rays point at the same part actor
        vtkActor* leftHit = rays->left.hoveredActor;
        vtkActor* rightHit = rays->right.hoveredActor;

        // accept if at least one ray hits a part actor
        vtkActor* target = nullptr;
        if (leftHit && isPartActor(leftHit, activeActors))
            target = leftHit;
        else if (rightHit && isPartActor(rightHit, activeActors))
            target = rightHit;

        if (!target) {
            active = false;
            targetActor = nullptr;
            return;
        }

        // get controller world positions from OpenVR tracking
        vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
        vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
            vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);

        auto getPos = [&](vr::TrackedDeviceIndex_t idx, double out[3]) -> bool {
            if (idx >= vr::k_unMaxTrackedDeviceCount || !poses[idx].bPoseIsValid)
                return false;
            auto& m = poses[idx].mDeviceToAbsoluteTracking;
            out[0] = m.m[0][3]; out[1] = m.m[1][3]; out[2] = m.m[2][3];
            return true;
        };

        double lp[3], rp[3];
        if (!getPos(leftIdx, lp) || !getPos(rightIdx, rp))
            return;

        double dx = lp[0]-rp[0], dy = lp[1]-rp[1], dz = lp[2]-rp[2];
        double curDist = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (!active || targetActor != target) {
            // starting a new scale gesture
            active = true;
            targetActor = target;
            startDist = curDist;
            double* s = target->GetScale();
            startScale[0] = s[0]; startScale[1] = s[1]; startScale[2] = s[2];
            return;
        }

        // ongoing scale — apply ratio
        if (startDist > 1e-4) {
            double ratio = curDist / startDist;
            targetActor->SetScale(
                startScale[0] * ratio,
                startScale[1] * ratio,
                startScale[2] * ratio);
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

void VRRenderThread::addActorOffline(vtkActor* actor, int partID) {
    if (!actor) return;
    m_pendingActors.emplace_back(partID, vtkSmartPointer<vtkActor>(actor));
}

void VRRenderThread::issueCommand(Command c, int partID, const QVariant& data) {
    QMutexLocker locker(&m_mutex);
    m_commandQueue.enqueue({ c, partID, data, nullptr });
}

void VRRenderThread::issueCommand(Command c, int partID, vtkActor* actor) {
    QMutexLocker locker(&m_mutex);
    m_commandQueue.enqueue({ c, partID, QVariant(), vtkSmartPointer<vtkActor>(actor) });
}

void VRRenderThread::run() {
    m_renderer      = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow  = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor    = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    const std::string manifestPath =
        (QCoreApplication::applicationDirPath() + "/vrbindings/vtk_openvr_actions.json").toStdString();
    m_interactor->SetActionManifestFileName(manifestPath.c_str());
    m_interactor->SetActionSetName("/actions/vtk");

    vtkNew<VRCustomStyle> customStyle;
    m_interactor->SetInteractorStyle(customStyle);

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;
    }
    m_pendingActors.clear();

    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // dual-controller ray casting + outline highlight
    vtkNew<VRRayCallback> rayCallback;
    rayCallback->init(m_renderer);
    m_interactor->AddObserver(vtkCommand::Move3DEvent, rayCallback, 1.0);

    m_renderWindow->Initialize();

    try {
        m_interactor->Initialize();
    } catch (...) {
    }

    // load 3D garage environment if available, otherwise fall back to skybox
    // must be after Initialize() so textures have an OpenGL context
    QString glbPath;
    QStringList glbNames = { "garage__warehouse.glb", "garage.glb" };
    QStringList searchDirs = {
        QCoreApplication::applicationDirPath(),
        QCoreApplication::applicationDirPath() + "/.."
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
        gltfImporter->Update();

        // collect environment actors and mark non-pickable
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

        // auto-scale garage to wrap around parts (STLs typically in mm, GLTF in metres)
        if (!envActors.empty() && !m_activeActors.empty()) {
            double envBounds[6] = { 1e30, -1e30, 1e30, -1e30, 1e30, -1e30 };
            for (auto* ea : envActors) {
                double b[6]; ea->GetBounds(b);
                for (int i = 0; i < 3; i++) {
                    if (b[i*2]   < envBounds[i*2])   envBounds[i*2]   = b[i*2];
                    if (b[i*2+1] > envBounds[i*2+1]) envBounds[i*2+1] = b[i*2+1];
                }
            }

            double partBounds[6] = { 1e30, -1e30, 1e30, -1e30, 1e30, -1e30 };
            for (auto& [id, partAct] : m_activeActors) {
                double b[6]; partAct->GetBounds(b);
                for (int i = 0; i < 3; i++) {
                    if (b[i*2]   < partBounds[i*2])   partBounds[i*2]   = b[i*2];
                    if (b[i*2+1] > partBounds[i*2+1]) partBounds[i*2+1] = b[i*2+1];
                }
            }

            double envSize = std::max({ envBounds[1]-envBounds[0],
                                        envBounds[3]-envBounds[2],
                                        envBounds[5]-envBounds[4] });
            double partSize = std::max({ partBounds[1]-partBounds[0],
                                         partBounds[3]-partBounds[2],
                                         partBounds[5]-partBounds[4] });

            if (envSize > 1e-6 && partSize > 1e-6) {
                double targetEnvSize = partSize * 5.0;
                double scaleFactor = targetEnvSize / envSize;
                for (auto* ea : envActors)
                    ea->SetScale(scaleFactor, scaleFactor, scaleFactor);
            }

            // recompute env bounds after scaling, centre garage around parts
            double envBounds2[6] = { 1e30, -1e30, 1e30, -1e30, 1e30, -1e30 };
            for (auto* ea : envActors) {
                double b[6]; ea->GetBounds(b);
                for (int i = 0; i < 3; i++) {
                    if (b[i*2]   < envBounds2[i*2])   envBounds2[i*2]   = b[i*2];
                    if (b[i*2+1] > envBounds2[i*2+1]) envBounds2[i*2+1] = b[i*2+1];
                }
            }

            double offX = (partBounds[0]+partBounds[1])*0.5 - (envBounds2[0]+envBounds2[1])*0.5;
            double offY = partBounds[2] - envBounds2[2];
            double offZ = (partBounds[4]+partBounds[5])*0.5 - (envBounds2[4]+envBounds2[5])*0.5;

            for (auto* ea : envActors) {
                double* pos = ea->GetPosition();
                ea->SetPosition(pos[0] + offX, pos[1] + offY, pos[2] + offZ);
            }
        }

        garageLoaded = true;
    }

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

    if (m_renderer->GetActiveCamera())
        m_renderer->ResetCameraClippingRange();

    // position user standing on the floor at the centre of the parts
    {
        double bounds[6];
        m_renderer->ComputeVisiblePropBounds(bounds);
        double* trans = m_renderWindow->GetPhysicalTranslation();
        double floorY = bounds[2];
        m_renderWindow->SetPhysicalTranslation(trans[0], floorY, trans[2]);
    }

    // per-actor scale system (two-grip pinch on a specific actor)
    PerActorScaler scaler;

    m_endRender = false;
    while (!m_endRender) {
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();
        scaler.update(m_renderWindow, rayCallback, &m_activeActors);
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
