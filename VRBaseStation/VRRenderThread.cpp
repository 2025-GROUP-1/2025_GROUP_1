
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
#include <vtkJPEGReader.h>
#include <vtkTexture.h>
#include <vtkSkybox.h>
#include <vtkCommand.h>
#include <vtkEventData.h>
#include <vtkProp3D.h>
#include <vtkOpenVRInteractorStyle.h>
#include <vtkPropPicker.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkActor.h>
#include <vtkCellPicker.h>

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
// ==========================================
// SHARED MEMORY POINTER
// ==========================================
static vtkProp3D* g_HoveredActor = nullptr;

// ==========================================
// 1. VR SCALE COMMAND (TRACKPAD SWIPE)
// ==========================================
class VRScaleCommand : public vtkCommand {
public:
    static VRScaleCommand* New() { return new VRScaleCommand; }

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        if (eventId == vtkCommand::Elevation3DEvent) {
            vtkEventData* edata = static_cast<vtkEventData*>(callData);
            vtkEventDataDevice3D* ed = edata->GetAsEventDataDevice3D();

            if (ed && ed->GetDevice() == vtkEventDataDevice::RightController) {

                // ==========================================
                // CRITICAL FIX 1: THE KILL SWITCH
                // This stops VTK from pushing/pulling the object or the camera!
                this->AbortFlagOn();
                // ==========================================

                if (g_HoveredActor) {
                    const double* trackpad = ed->GetTrackPadPosition();
                    double swipeY = trackpad[1];

                    double currentScale[3];
                    g_HoveredActor->GetScale(currentScale);
                    double newScale = currentScale[0] + (swipeY * 0.0001);

                    if (newScale > 0.00001) {
                        g_HoveredActor->SetScale(newScale, newScale, newScale);
                    }
                }
            }
        }
    }
};

// ==========================================
// 2. VR HOVER COMMAND (BLUE GLOW)
// ==========================================
class VRHoverCommand : public vtkCommand {
public:
    static VRHoverCommand* New() { return new VRHoverCommand; }

    vtkRenderer* renderer = nullptr;
    vtkProp3D* lastHoveredActor = nullptr;

    // ==========================================
    // CRITICAL FIX 2: PRECISE TRIANGLE PICKING
    // Upgraded from vtkPropPicker so it phases right through invisible bounding boxes!
    vtkNew<vtkCellPicker> picker;
    // ==========================================

    void Execute(vtkObject* caller, unsigned long eventId, void* callData) override {
        if (eventId == vtkCommand::Move3DEvent) {
            vtkEventData* edata = static_cast<vtkEventData*>(callData);
            vtkEventDataDevice3D* ed = edata->GetAsEventDataDevice3D();

            if (ed && renderer) {
                // Ignore the Left Controller
                if (ed->GetDevice() != vtkEventDataDevice::RightController) return;

                const double* constPos = ed->GetWorldPosition();
                const double* constOri = ed->GetWorldOrientation();

                double pos[3] = { constPos[0], constPos[1], constPos[2] };
                double ori[4] = { constOri[0], constOri[1], constOri[2], constOri[3] };

                picker->SetTolerance(0.0); // Tell the raycast to be pixel-perfect
                picker->Pick3DRay(pos, ori, renderer);
                vtkProp3D* targetActor = picker->GetProp3D();

                if (targetActor != lastHoveredActor) {
                    // Turn off old glow
                    if (lastHoveredActor) {
                        vtkActor* oldActor = vtkActor::SafeDownCast(lastHoveredActor);
                        if (oldActor) oldActor->GetProperty()->SetAmbient(0.0);
                    }
                    // Turn on new glow
                    if (targetActor) {
                        vtkActor* newActor = vtkActor::SafeDownCast(targetActor);
                        if (newActor) {
                            newActor->GetProperty()->SetAmbientColor(0.0, 0.0, 1.0);
                            newActor->GetProperty()->SetAmbient(0.6);
                        }
                    }

                    lastHoveredActor = targetActor;
                    g_HoveredActor = targetActor;
                }
            }
        }
    }
};
// --------------------------------------
void VRRenderThread::run() {
    /// 1. Force Simulator mode again just in case
    _putenv_s("VTK_VR_SIMULATOR", "1");

    m_renderer = vtkSmartPointer<vtkOpenVRRenderer>::New();
    m_renderWindow = vtkSmartPointer<vtkOpenVRRenderWindow>::New();
    m_interactor = vtkSmartPointer<vtkOpenVRRenderWindowInteractor>::New();

    m_renderWindow->AddRenderer(m_renderer);
    m_interactor->SetRenderWindow(m_renderWindow);

    // ==========================================
    // STEP 1: SET UP THE STRICT RAYCAST FILTER
    // ==========================================
    vtkNew<VRHoverCommand> hoverCmd;
    hoverCmd->renderer = m_renderer;
    hoverCmd->picker->PickFromListOn(); // Tell the raycast to ignore everything by default!

    // 3. Register your actors AND whitelist them
    for (auto& [id, actor] : m_pendingActors) {
        m_renderer->AddActor(actor);
        m_activeActors[id] = actor;

        // Add ONLY the STLs to the filter so the raycast can see them
        hoverCmd->picker->AddPickList(actor);
    }
    m_pendingActors.clear();

    // ==========================================
    // STEP 2: ATTACH THE LISTENERS
    // ==========================================
    m_interactor->AddObserver(vtkCommand::Move3DEvent, hoverCmd);

    vtkNew<VRScaleCommand> scaleCmd;
    m_interactor->AddObserver(vtkCommand::Elevation3DEvent, scaleCmd);

    // 4. Setup the camera so you aren't blind
    m_renderer->ResetCamera();
    m_renderer->ResetCameraClippingRange();

    // ... KEEP YOUR SKYBOX AND RENDER LOOP EXACTLY AS THEY ARE BELOW THIS! ...
    
    // --- 360 ROOM BACKGROUND (SKYBOX) ---
    // --- 360 ROOM BACKGROUND (SKYBOX) ---
    std::string roomPath = "C:\\Users\\eeysm11\\Downloads\\2025_GROUP_1-feature-sk-vr-thread-header\\VRBaseStation\\room.jpg";

    vtkNew<vtkJPEGReader> bgReader;

    // SAFETY NET: Check if the file actually exists and can be read!
    if (bgReader->CanReadFile(roomPath.c_str())) {
        bgReader->SetFileName(roomPath.c_str());
        bgReader->Update();

        vtkNew<vtkTexture> bgTexture;
        bgTexture->SetInputConnection(bgReader->GetOutputPort());
        bgTexture->InterpolateOn();

        vtkNew<vtkSkybox> skybox;
        skybox->SetTexture(bgTexture);
        skybox->SetProjectionToSphere();
        skybox->PickableOff(); // Prevent laser from hitting the background

        m_renderer->AddActor(skybox);
    }
    else {
        // If the path is wrong, skip the skybox so the app doesn't crash!
        qDebug("WARNING: Could not find room.jpg! Check your file path.");
    }
    // ------------------------------------

    m_renderWindow->Initialize();

    // 5. Main VR loop.
    m_endRender = false;
    while (!m_endRender) {
        m_renderWindow->Render();
        m_interactor->DoOneEvent(m_renderWindow, m_renderer);
        processCommands();
    }

    // 6. Graceful shutdown - release the HMD.
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