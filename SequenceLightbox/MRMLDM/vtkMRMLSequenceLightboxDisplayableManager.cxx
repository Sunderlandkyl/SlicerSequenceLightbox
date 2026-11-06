/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Sequences MRMLDM includes
#include "vtkMRMLSequenceLightboxDisplayableManager.h"

// Sequences includes
#include "vtkMRMLSequenceBrowserNode.h"
#include "vtkMRMLSequenceLightboxViewNode.h"
#include "vtkSequenceLightboxSceneMirror.h"

// MRMLDisplayableManager includes
#include <vtkMRMLAccuratePicker.h>
#include <vtkMRMLDisplayableManagerFactory.h>
#include <vtkMRMLDisplayableManagerGroup.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLInteractionEventData.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLSliceLogic.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkEvent.h>
#include <vtkCoordinate.h>
#include <vtkAlgorithmOutput.h>
#include <vtkImageMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkSmartPointer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkWorldPointPicker.h>

// STD includes
#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLSequenceLightboxDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLSequenceLightboxDisplayableManager::vtkInternal
{
public:
  /// A single tile of the lightbox: a complete, independent view rendered into a viewport
  /// of the lightbox view's render window.
  struct Tile
  {
    vtkSmartPointer<vtkRenderer> Renderer;
    vtkSmartPointer<vtkSequenceLightboxSceneMirror> Mirror;
    vtkSmartPointer<vtkMRMLApplicationLogic> AppLogic;
    vtkSmartPointer<vtkMRMLDisplayableManagerFactory> Factory;
    vtkSmartPointer<vtkMRMLDisplayableManagerGroup> Group;
    unsigned long GroupObservationTag{ 0 };

    /// Slice views only: the pipeline that produces the slice image of this tile.
    vtkSmartPointer<vtkMRMLSliceLogic> SliceLogic;
    vtkSmartPointer<vtkCollection> SliceLogics;
    vtkSmartPointer<vtkImageMapper> ImageMapper;
    vtkSmartPointer<vtkActor2D> ImageActor;

    vtkSmartPointer<vtkTextActor> LabelActor;
    vtkSmartPointer<vtkActor2D> HighlightActor;
    /// Frame drawn around the tile that is under the mouse pointer, so that it is clear which tile
    /// receives the interaction.
    vtkSmartPointer<vtkActor2D> HoverFrameActor;

    /// Renderers that the displayable managers of the tile added to the render window (for
    /// example the renderer of the interaction handles of markups). They are not aware of the
    /// tiles: they are rendered into the viewport of the tile and removed together with it.
    std::vector<vtkSmartPointer<vtkRenderer>> OverlayRenderers;
    /// Viewport that was last assigned to the overlay renderers. An overlay renderer that its
    /// owner has positioned since then (for example in a corner of the view) is left alone.
    double OverlayViewport[4]{ 0.0, 0.0, 1.0, 1.0 };

    /// Displayable manager of this tile that processed the last interaction event.
    vtkWeakPointer<vtkMRMLAbstractDisplayableManager> FocusedDisplayableManager;

    int ItemNumber{ -1 };
  };

  vtkInternal(vtkMRMLSequenceLightboxDisplayableManager* external);
  ~vtkInternal();

  vtkMRMLSequenceLightboxViewNode* GetViewNode();

  /// Create, destroy and update the tiles so that they match the view node.
  void UpdateTiles();
  /// Update the viewport (and, for slice views, the size) of every tile.
  void UpdateTileLayout();
  /// Update the content of every tile: sequence item, slice image, label, highlight.
  void UpdateTileContent();
  /// Reorder the tiles so that a tile that already shows the item that is needed at a position
  /// is moved to that position instead of being updated to a different item. During playback (or
  /// when the grid is scrolled) most items only move to a neighboring tile: moving the tile only
  /// changes its viewport, while changing its item would recompute the slice image of the tile.
  /// Returns true if any tile has been moved.
  bool ReuseTilesForItems();

  bool CreateTile(Tile& tile, vtkMRMLAbstractViewNode* referenceViewNode, vtkMRMLSequenceBrowserNode* browserNode);
  void DestroyTile(Tile& tile);
  void DestroyAllTiles();

  /// Returns the factory whose displayable managers render the content of the tiles.
  vtkMRMLDisplayableManagerFactory* GetSourceDisplayableManagerFactory(bool threeDMode);

  int GetTileAtDisplayPosition(int x, int y);

  /// Returns true if the tiles process interaction events like regular views.
  bool IsTileInteractionEnabled();
  /// Returns the index of the tile that the event has to be forwarded to, or -1.
  int GetTargetTileIndex(vtkMRMLInteractionEventData* eventData);
  /// Make the specified tile the one under the mouse pointer: the previously hovered tile is
  /// notified that it lost focus. Pass -1 if the pointer is not over any tile.
  void SetHoveredTileIndex(int tileIndex, vtkMRMLInteractionEventData* eventData);
  /// Show the hover frame of the hovered tile only.
  void UpdateHoverFrames();
  /// Create a 2D actor that draws the border of the viewport of the renderer.
  static vtkSmartPointer<vtkActor2D> CreateFrameActor(vtkRenderer* renderer, float lineWidth);
  /// Step through the items with the keyboard. Returns true if the key was used.
  bool ProcessKeyPressEvent(vtkMRMLInteractionEventData* eventData);
  /// Remember where a mouse button was pressed, to tell a click from a drag.
  void SetPressPosition(vtkMRMLInteractionEventData* eventData);
  /// Returns true if the button release completes a click: the pointer is within ClickTolerance
  /// pixels of the position where the button was pressed. The pointer of a physical mouse often
  /// moves by a pixel during a click, so requiring no movement at all would ignore some clicks.
  bool IsClick(vtkMRMLInteractionEventData* eventData);
  /// Forward the event to the displayable managers of the tile. Returns true if it was processed.
  bool DelegateInteractionEventToTile(int tileIndex, vtkMRMLInteractionEventData* eventData);
  /// Translate an event of the lightbox view to an event of the view that the tile renders.
  void CreateTileEventData(Tile& tile, vtkMRMLInteractionEventData* eventData, vtkMRMLInteractionEventData* tileEventData);
  /// Let the most suitable displayable manager of the tile process the event, writing the
  /// changes that it makes in the tile scene back to the scene of the application.
  bool ProcessTileEventData(Tile& tile, vtkMRMLInteractionEventData* tileEventData);

  void AddRendererUpdateObserver(vtkRenderer* renderer);
  void RemoveRendererUpdateObserver();

  /// Request a render once the render that is in progress has finished. Used when a
  /// synchronization is pending while the view is being rendered directly (not through the
  /// view widget, which synchronizes the tiles before rendering): the tiles cannot be
  /// synchronized during the render, so the view is rendered again afterwards.
  void RequestRenderAfterCurrentRender();
  /// Invoked by the one-shot timer created by RequestRenderAfterCurrentRender.
  static void TimerCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);
  /// Returns true if the render window of the view is being rendered.
  bool IsRenderInProgress();

  /// Invoked when the content of a tile changed and the tiles have to be synchronized.
  static void ContentModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);
  /// Synchronize the content of the tiles if a synchronization has been requested since the last
  /// one. Called before the view is rendered and before an interaction event is processed.
  void UpdateTileContentIfRequested();
  /// Returns true if the tiles have to be created, destroyed or rebuilt: they do not exist although
  /// the view node is fully configured, or they were built for another reference view or browser
  /// node. Such changes are applied immediately (see UpdateTiles), while changes of the content of
  /// the tiles are applied right before the next render (see UpdateTileContentIfRequested).
  bool IsTileStructureOutdated();
  /// Synchronize the tiles with the scene, now if their structure is outdated and right before the
  /// next render otherwise.
  void RequestTileUpdate();
  /// Invoked when a tile only needs to be drawn again, without synchronizing its content.
  static void RenderRequestCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);
  /// Ask for the view to be drawn again. While the tiles are being synchronized the request is
  /// recorded instead (see RequestRenderAfterUpdate): the view could otherwise be rendered
  /// immediately, with tiles that are only partially synchronized.
  void RequestRender();
  /// Request the render that the tiles asked for while they were synchronized, unless the view
  /// is about to be rendered anyway.
  void RequestRenderAfterUpdate();
  /// Invoked at the beginning of each render of the lightbox view, to detect view size changes.
  static void RendererUpdateCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  vtkMRMLSequenceLightboxDisplayableManager* External;

  std::vector<Tile> Tiles;

  /// Configuration that the current tiles were created for. If any of these change then the
  /// tiles are rebuilt from scratch.
  vtkWeakPointer<vtkMRMLAbstractViewNode> ReferenceViewNode;
  vtkWeakPointer<vtkMRMLSequenceBrowserNode> BrowserNode;
  int RegisteredDisplayableManagerCount{ -1 };

  int LastViewSize[2] = { 0, 0 };

  vtkSmartPointer<vtkCallbackCommand> ContentModifiedCallbackCommand;
  vtkSmartPointer<vtkCallbackCommand> RenderRequestCallbackCommand;
  vtkSmartPointer<vtkCallbackCommand> RendererUpdateCallbackCommand;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;
  unsigned long RendererObservationTag{ 0 };

  vtkSmartPointer<vtkCallbackCommand> TimerCallbackCommand;
  vtkWeakPointer<vtkRenderWindowInteractor> TimerInteractor;
  unsigned long TimerObservationTag{ 0 };
  int PendingRenderTimerId{ 0 };

  bool UpdatingTiles{ false };

  /// True if a node that is displayed in a tile has been modified since the tiles were last
  /// synchronized. The synchronization is performed once, right before the view is rendered:
  /// a single change of the application (for example selecting another sequence item, which
  /// modifies every proxy node) modifies many mirrored nodes, and synchronizing every tile after
  /// each of these modifications would make the cost of a change proportional to the number of
  /// modified nodes times the number of tiles.
  bool ContentUpdateRequested{ false };

  /// True while an interaction event is forwarded to the displayable managers of a tile. Tile
  /// updates that are requested in the meantime (writing back the changes of the tile to the
  /// application scene modifies nodes that every tile mirrors) are performed once, after the
  /// event has been processed: the tile that processes the event must not be destroyed or
  /// synchronized while its displayable managers are running, and a single update is enough.
  bool ProcessingTileEvent{ false };
  bool TileUpdateDeferred{ false };

  /// True if a tile asked to be drawn again while the tiles were being synchronized.
  bool RenderRequestedDuringUpdate{ false };
  /// True while the tiles are synchronized by the view, right before it is rendered: render
  /// requests are not needed then.
  bool SuppressRenderRequests{ false };

  /// Pickers used to compute the world position of events in 3D tiles. They are shared by the
  /// tiles, a pick is always performed in the renderer of a specific tile.
  vtkSmartPointer<vtkMRMLAccuratePicker> AccuratePicker;
  vtkSmartPointer<vtkWorldPointPicker> QuickPicker;

  /// Tile that received the last mouse button press. It receives all events until the button is
  /// released, even if the pointer leaves the tile (for example while dragging a markup).
  int ActiveTileIndex{ -1 };
  /// True if a displayable manager of the active tile processed the button press. In that case
  /// the button release does not select the item of the tile.
  bool ActiveTilePressProcessed{ false };
  /// Tile under the mouse pointer.
  int HoveredTileIndex{ -1 };
  /// Position of the last mouse button press, see IsClick.
  int PressDisplayPosition[2]{ 0, 0 };
  bool PressDisplayPositionValid{ false };
  /// Maximum distance (in pixels) between the press and the release of a click, the same as the
  /// default drag start distance of Qt.
  static constexpr int ClickTolerance = 10;
};

//---------------------------------------------------------------------------
vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::vtkInternal(vtkMRMLSequenceLightboxDisplayableManager* external)
  : External(external)
{
  this->ContentModifiedCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->ContentModifiedCallbackCommand->SetClientData(this);
  this->ContentModifiedCallbackCommand->SetCallback(vtkInternal::ContentModifiedCallback);

  this->RenderRequestCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->RenderRequestCallbackCommand->SetClientData(this);
  this->RenderRequestCallbackCommand->SetCallback(vtkInternal::RenderRequestCallback);

  this->RendererUpdateCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->RendererUpdateCallbackCommand->SetClientData(this);
  this->RendererUpdateCallbackCommand->SetCallback(vtkInternal::RendererUpdateCallback);

  this->TimerCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->TimerCallbackCommand->SetClientData(this);
  this->TimerCallbackCommand->SetCallback(vtkInternal::TimerCallback);

  // Same pickers as the ones of vtkMRMLThreeDViewInteractorStyle.
  this->AccuratePicker = vtkSmartPointer<vtkMRMLAccuratePicker>::New();
  this->AccuratePicker->SetTolerance(.005);
  this->QuickPicker = vtkSmartPointer<vtkWorldPointPicker>::New();
}

//---------------------------------------------------------------------------
vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::~vtkInternal()
{
  this->RemoveRendererUpdateObserver();
  this->DestroyAllTiles();
  if (this->TimerInteractor)
  {
    if (this->PendingRenderTimerId)
    {
      this->TimerInteractor->DestroyTimer(this->PendingRenderTimerId);
    }
    this->TimerInteractor->RemoveObserver(this->TimerObservationTag);
  }
  this->ContentModifiedCallbackCommand->SetClientData(nullptr);
  this->RenderRequestCallbackCommand->SetClientData(nullptr);
  this->RendererUpdateCallbackCommand->SetClientData(nullptr);
  this->TimerCallbackCommand->SetClientData(nullptr);
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::IsRenderInProgress()
{
  vtkRenderer* renderer = this->External->GetRenderer();
  vtkRenderWindow* renderWindow = renderer ? renderer->GetRenderWindow() : nullptr;
  return renderWindow != nullptr && renderWindow->CheckInRenderStatus();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RequestRenderAfterCurrentRender()
{
  vtkRenderWindowInteractor* interactor = this->External->GetInteractor();
  if (!interactor)
  {
    return;
  }
  if (this->TimerInteractor != interactor)
  {
    if (this->TimerInteractor)
    {
      if (this->PendingRenderTimerId)
      {
        this->TimerInteractor->DestroyTimer(this->PendingRenderTimerId);
        this->PendingRenderTimerId = 0;
      }
      this->TimerInteractor->RemoveObserver(this->TimerObservationTag);
    }
    this->TimerInteractor = interactor;
    this->TimerObservationTag = interactor->AddObserver(vtkCommand::TimerEvent, this->TimerCallbackCommand);
  }
  if (this->PendingRenderTimerId)
  {
    // Already requested.
    return;
  }
  // The timer event is delivered by the event loop, after the current render has finished.
  this->PendingRenderTimerId = interactor->CreateOneShotTimer(1);
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::TimerCallback(vtkObject* vtkNotUsed(caller), unsigned long vtkNotUsed(eid), void* clientData, void* callData)
{
  vtkInternal* self = reinterpret_cast<vtkInternal*>(clientData);
  if (!self || !self->External || !callData || !self->PendingRenderTimerId)
  {
    return;
  }
  int timerId = *reinterpret_cast<int*>(callData);
  if (timerId != self->PendingRenderTimerId)
  {
    return;
  }
  self->PendingRenderTimerId = 0;
  if (self->ContentUpdateRequested)
  {
    // The render goes through the view widget, which synchronizes the tiles first.
    self->External->RequestRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::ContentModifiedCallback(vtkObject* vtkNotUsed(caller),
                                                                                     unsigned long vtkNotUsed(eid),
                                                                                     void* clientData,
                                                                                     void* vtkNotUsed(callData))
{
  vtkInternal* self = reinterpret_cast<vtkInternal*>(clientData);
  if (!self || !self->External || self->UpdatingTiles)
  {
    // Changes that this class makes while it synchronizes the tiles must not request another
    // synchronization, otherwise a single change of the reference view would synchronize the
    // tiles again and again.
    return;
  }
  // A scene mirror reports that a node that is displayed in a tile has been modified. The
  // content of the tiles is synchronized right before the next render (see SynchronizeTiles),
  // not here: RequestRender() would perform the update immediately, once for each modified node.
  self->ContentUpdateRequested = true;
  self->External->RequestRender();
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::IsTileStructureOutdated()
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  vtkMRMLAbstractViewNode* referenceViewNode = (viewNode ? viewNode->GetReferenceViewNode() : nullptr);
  vtkMRMLSequenceBrowserNode* browserNode = (viewNode ? viewNode->GetSequenceBrowserNode() : nullptr);
  if (this->Tiles.empty())
  {
    // Tiles can be created if the view node is fully configured.
    return (referenceViewNode != nullptr && browserNode != nullptr);
  }
  return referenceViewNode != this->ReferenceViewNode.GetPointer() || browserNode != this->BrowserNode.GetPointer();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RequestTileUpdate()
{
  if (this->IsTileStructureOutdated())
  {
    // Tiles are created and destroyed immediately: adding renderers to the render window (or
    // removing them) is not possible while the window is being rendered.
    this->External->SetUpdateFromMRMLRequested(true);
    this->External->RequestRender();
    return;
  }
  // Only the content of the tiles has to be synchronized, which is done once before the next
  // render, however many changes are made in the meantime.
  this->ContentUpdateRequested = true;
  this->External->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::UpdateTileContentIfRequested()
{
  if (this->UpdatingTiles || !this->ContentUpdateRequested)
  {
    return;
  }
  if (this->ProcessingTileEvent)
  {
    // The tile that processes the event must not be synchronized while its displayable managers
    // are running. The tiles are updated once the event has been processed.
    this->TileUpdateDeferred = true;
    return;
  }
  if (this->IsRenderInProgress())
  {
    // Some displayable managers ignore the modifications that are made while the view is being
    // rendered, so the tiles would not show the changes. The view is rendered again (through the
    // view widget, which synchronizes the tiles first) once the current render has finished.
    this->RequestRenderAfterCurrentRender();
    return;
  }
  this->UpdatingTiles = true;
  this->ContentUpdateRequested = false;
  this->UpdateTileContent();
  this->UpdatingTiles = false;
  this->RequestRenderAfterUpdate();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RenderRequestCallback(vtkObject* vtkNotUsed(caller),
                                                                                   unsigned long vtkNotUsed(eid),
                                                                                   void* clientData,
                                                                                   void* vtkNotUsed(callData))
{
  vtkInternal* self = reinterpret_cast<vtkInternal*>(clientData);
  if (!self || !self->External)
  {
    return;
  }
  // A displayable manager of a tile (or the slice pipeline of a tile) asks for the view to be
  // drawn again. This does not mean that the content of the tiles is out of date, so no
  // synchronization is requested: doing so would make every tile request a synchronization of
  // every other tile.
  self->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RequestRender()
{
  if (this->UpdatingTiles)
  {
    // The view widget may render the view immediately when a render is requested. The tiles
    // that are not synchronized yet would show their old content for a frame.
    this->RenderRequestedDuringUpdate = true;
    return;
  }
  this->External->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RequestRenderAfterUpdate()
{
  if (!this->RenderRequestedDuringUpdate)
  {
    return;
  }
  this->RenderRequestedDuringUpdate = false;
  if (this->SuppressRenderRequests)
  {
    // The view is rendered right after the synchronization.
    return;
  }
  this->External->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RendererUpdateCallback(vtkObject* vtkNotUsed(caller),
                                                                                    unsigned long vtkNotUsed(eid),
                                                                                    void* clientData,
                                                                                    void* vtkNotUsed(callData))
{
  vtkInternal* self = reinterpret_cast<vtkInternal*>(clientData);
  if (!self || !self->External || self->UpdatingTiles)
  {
    return;
  }
  vtkRenderer* renderer = self->External->GetRenderer();
  vtkRenderWindow* renderWindow = renderer ? renderer->GetRenderWindow() : nullptr;
  if (!renderWindow)
  {
    return;
  }
  const int* size = renderWindow->GetSize();
  if (size[0] == self->LastViewSize[0] && size[1] == self->LastViewSize[1])
  {
    return;
  }
  // The view has been resized: the viewports of the tiles must follow. The content of the tiles
  // (including the slice image size) is synchronized before the next render, which is requested
  // by the mirrors when the slice dimensions change; the content is not synchronized here,
  // because this is called while the view is being rendered, and some displayable managers
  // ignore the modifications made while rendering is in progress.
  self->UpdateTileLayout();
  if (self->ContentUpdateRequested)
  {
    self->RequestRenderAfterCurrentRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::AddRendererUpdateObserver(vtkRenderer* renderer)
{
  this->RemoveRendererUpdateObserver();
  if (renderer)
  {
    this->ObservedRenderer = renderer;
    this->RendererObservationTag = renderer->AddObserver(vtkCommand::StartEvent, this->RendererUpdateCallbackCommand);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::RemoveRendererUpdateObserver()
{
  if (this->ObservedRenderer)
  {
    this->ObservedRenderer->RemoveObserver(this->RendererObservationTag);
    this->RendererObservationTag = 0;
    this->ObservedRenderer = nullptr;
  }
}

//---------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode* vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::GetViewNode()
{
  return vtkMRMLSequenceLightboxViewNode::SafeDownCast(this->External->GetMRMLDisplayableNode());
}

//---------------------------------------------------------------------------
vtkMRMLDisplayableManagerFactory* vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::GetSourceDisplayableManagerFactory(bool threeDMode)
{
  if (threeDMode)
  {
    return vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance();
  }
  return vtkMRMLSliceViewDisplayableManagerFactory::GetInstance();
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::CreateTile(Tile& tile, vtkMRMLAbstractViewNode* referenceViewNode, vtkMRMLSequenceBrowserNode* browserNode)
{
  vtkRenderer* mainRenderer = this->External->GetRenderer();
  vtkRenderWindow* renderWindow = mainRenderer ? mainRenderer->GetRenderWindow() : nullptr;
  if (!renderWindow || !referenceViewNode || !browserNode)
  {
    return false;
  }

  // The renderer must already be in the render window when the displayable managers are
  // instantiated, because they retrieve the interactor from it.
  tile.Renderer = vtkSmartPointer<vtkRenderer>::New();
  tile.Renderer->InteractiveOff();
  tile.Renderer->SetLayer(mainRenderer->GetLayer());
  // Make sure that a camera is created and associated with the renderer, so that the
  // display coordinate conversions of the 2D actors work as expected.
  if (vtkCamera* camera = tile.Renderer->GetActiveCamera())
  {
    camera->ParallelProjectionOn();
  }
  renderWindow->AddRenderer(tile.Renderer);

  // Private scene that contains the content of this tile.
  tile.Mirror = vtkSmartPointer<vtkSequenceLightboxSceneMirror>::New();
  tile.Mirror->SetSourceScene(this->External->GetMRMLScene());
  tile.Mirror->SetSequenceBrowserNode(browserNode);
  tile.Mirror->SetReferenceViewNode(referenceViewNode);
  tile.Mirror->Update();
  tile.Mirror->AddObserver(vtkCommand::ModifiedEvent, this->ContentModifiedCallbackCommand);

  vtkMRMLScene* tileScene = tile.Mirror->GetTileScene();
  vtkMRMLAbstractViewNode* tileViewNode = tile.Mirror->GetTileViewNode();
  if (!tileScene || !tileViewNode)
  {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLSequenceLightboxDisplayableManager: failed to mirror the reference view");
    return false;
  }

  // Each tile has its own application logic, so that the logics and displayable managers of
  // the tile operate on the mirror scene instead of the scene of the application.
  tile.AppLogic = vtkSmartPointer<vtkMRMLApplicationLogic>::New();
  vtkMRMLApplicationLogic* mainAppLogic = this->External->GetMRMLApplicationLogic();
  if (mainAppLogic)
  {
    tile.AppLogic->SetShareDirectory(mainAppLogic->GetShareDirectory());
    tile.AppLogic->SetHomeDirectory(mainAppLogic->GetHomeDirectory());
    tile.AppLogic->SetColorLogic(mainAppLogic->GetColorLogic());
    // Some displayable managers require the logic of their module.
    const char* moduleNames[] = { "Markups", "Segmentations", "VolumeRendering", "Colors", "Transforms", "Models", "Volumes" };
    for (const char* moduleName : moduleNames)
    {
      vtkMRMLAbstractLogic* moduleLogic = mainAppLogic->GetModuleLogic(moduleName);
      if (moduleLogic)
      {
        tile.AppLogic->SetModuleLogic(moduleName, moduleLogic);
      }
    }
  }
  tile.AppLogic->SetMRMLScene(tileScene);

  vtkMRMLSliceNode* tileSliceNode = vtkMRMLSliceNode::SafeDownCast(tileViewNode);
  if (tileSliceNode)
  {
    // Slice views: the slice image is produced by a slice logic of this tile and displayed
    // the same way as in a regular slice view (a 2D image mapper drawn in the background).
    tile.SliceLogic = vtkSmartPointer<vtkMRMLSliceLogic>::New();
    tile.SliceLogic->SetMRMLApplicationLogic(tile.AppLogic);
    tile.SliceLogic->SetMRMLScene(tileScene);
    tile.SliceLogic->SetSliceNode(tileSliceNode);
    tile.SliceLogic->UpdateSliceCompositeNode();
    // The slice pipeline of the tile only asks for the tile to be drawn again: the content of
    // the tiles is synchronized by the scene mirror.
    tile.SliceLogic->AddObserver(vtkCommand::ModifiedEvent, this->RenderRequestCallbackCommand);

    tile.SliceLogics = vtkSmartPointer<vtkCollection>::New();
    tile.SliceLogics->AddItem(tile.SliceLogic);
    tile.AppLogic->SetSliceLogics(tile.SliceLogics);

    tile.ImageMapper = vtkSmartPointer<vtkImageMapper>::New();
    tile.ImageMapper->SetColorWindow(255.);
    tile.ImageMapper->SetColorLevel(127.5);
    tile.ImageActor = vtkSmartPointer<vtkActor2D>::New();
    tile.ImageActor->SetMapper(tile.ImageMapper);
    tile.ImageActor->SetVisibility(false);
    // The image actor is added first, so that it is rendered below everything that the
    // displayable managers of the tile draw.
    tile.Renderer->AddActor2D(tile.ImageActor);
  }

  // The content of the tile is rendered by the standard displayable managers of a slice view
  // or a 3D view, including the ones that were registered by modules.
  vtkMRMLDisplayableManagerFactory* sourceFactory = this->GetSourceDisplayableManagerFactory(tileSliceNode == nullptr);
  tile.Factory = vtkSmartPointer<vtkMRMLDisplayableManagerFactory>::New();
  int numberOfDisplayableManagers = sourceFactory->GetRegisteredDisplayableManagerCount();
  for (int displayableManagerIndex = 0; displayableManagerIndex < numberOfDisplayableManagers; ++displayableManagerIndex)
  {
    tile.Factory->RegisterDisplayableManager(sourceFactory->GetRegisteredDisplayableManagerName(displayableManagerIndex).c_str());
  }
  tile.Factory->SetMRMLApplicationLogic(tile.AppLogic);
  // The displayable managers may add further renderers to the render window (for example the
  // markups displayable manager renders the interaction handles in a renderer of its own), which
  // are found by comparing the renderers of the window before and after their instantiation.
  std::set<vtkRenderer*> renderersBefore;
  vtkRendererCollection* renderers = renderWindow->GetRenderers();
  vtkCollectionSimpleIterator rendererIterator;
  renderers->InitTraversal(rendererIterator);
  while (vtkRenderer* renderer = renderers->GetNextRenderer(rendererIterator))
  {
    renderersBefore.insert(renderer);
  }
  tile.Group = vtkSmartPointer<vtkMRMLDisplayableManagerGroup>::Take(tile.Factory->InstantiateDisplayableManagers(tile.Renderer));
  // The displayable managers are created (which is when they add their renderers) once they
  // have both a renderer and a view node.
  tile.Group->SetMRMLDisplayableNode(tileViewNode);
  renderers->InitTraversal(rendererIterator);
  while (vtkRenderer* renderer = renderers->GetNextRenderer(rendererIterator))
  {
    if (renderer != tile.Renderer && renderersBefore.find(renderer) == renderersBefore.end())
    {
      tile.OverlayRenderers.push_back(renderer);
    }
  }
  tile.GroupObservationTag = tile.Group->AddObserver(vtkCommand::UpdateEvent, this->RenderRequestCallbackCommand);

  // Item label.
  tile.LabelActor = vtkSmartPointer<vtkTextActor>::New();
  tile.LabelActor->GetTextProperty()->SetFontSize(12);
  tile.LabelActor->GetTextProperty()->SetColor(1.0, 1.0, 1.0);
  tile.LabelActor->GetTextProperty()->SetShadow(1);
  tile.LabelActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
  tile.LabelActor->GetPositionCoordinate()->SetValue(0.03, 0.03);
  tile.LabelActor->SetVisibility(false);
  tile.Renderer->AddActor2D(tile.LabelActor);

  // Border that highlights the tile showing the currently selected item.
  tile.HighlightActor = vtkInternal::CreateFrameActor(tile.Renderer, 2.0f);
  tile.Renderer->AddActor2D(tile.HighlightActor);
  // Thin frame around the tile under the mouse pointer.
  tile.HoverFrameActor = vtkInternal::CreateFrameActor(tile.Renderer, 1.0f);
  tile.HoverFrameActor->GetProperty()->SetColor(0.6, 0.6, 0.6);
  tile.Renderer->AddActor2D(tile.HoverFrameActor);

  return true;
}

//---------------------------------------------------------------------------
vtkSmartPointer<vtkActor2D> vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::CreateFrameActor(vtkRenderer* renderer, float lineWidth)
{
  // 0.0002 shift so that the border is not clipped, as in the CTK light box renderer.
  const double shift = 0.0002;
  vtkNew<vtkPoints> points;
  points->InsertNextPoint(0. + shift, 0. + shift, 0.);
  points->InsertNextPoint(1. + shift, 0. + shift, 0.);
  points->InsertNextPoint(1. + shift, 1. + shift, 0.);
  points->InsertNextPoint(0. + shift, 1. + shift, 0.);
  vtkNew<vtkCellArray> cells;
  cells->InsertNextCell(5);
  cells->InsertCellPoint(0);
  cells->InsertCellPoint(1);
  cells->InsertCellPoint(2);
  cells->InsertCellPoint(3);
  cells->InsertCellPoint(0);
  vtkNew<vtkPolyData> polyData;
  polyData->SetPoints(points);
  polyData->SetLines(cells);

  vtkNew<vtkCoordinate> coordinate;
  coordinate->SetCoordinateSystemToNormalizedViewport();
  coordinate->SetViewport(renderer);

  vtkNew<vtkPolyDataMapper2D> polyDataMapper;
  polyDataMapper->SetInputData(polyData);
  polyDataMapper->SetTransformCoordinate(coordinate);
  polyDataMapper->SetTransformCoordinateUseDouble(true);

  vtkSmartPointer<vtkActor2D> frameActor = vtkSmartPointer<vtkActor2D>::New();
  frameActor->SetMapper(polyDataMapper);
  frameActor->GetProperty()->SetLineWidth(lineWidth);
  frameActor->GetProperty()->SetDisplayLocationToForeground();
  frameActor->SetVisibility(false);
  return frameActor;
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::DestroyTile(Tile& tile)
{
  if (tile.Group)
  {
    if (tile.GroupObservationTag)
    {
      tile.Group->RemoveObserver(tile.GroupObservationTag);
      tile.GroupObservationTag = 0;
    }
    // Releases the MRML observations and the actors of the displayable managers.
    tile.Group->SetMRMLDisplayableNode(nullptr);
  }
  if (tile.SliceLogic)
  {
    tile.SliceLogic->RemoveObservers(vtkCommand::ModifiedEvent, this->RenderRequestCallbackCommand);
    tile.SliceLogic->SetSliceNode(nullptr);
    tile.SliceLogic->SetMRMLScene(nullptr);
  }
  if (tile.AppLogic)
  {
    tile.AppLogic->SetSliceLogics(nullptr);
    tile.AppLogic->SetMRMLScene(nullptr);
  }
  if (tile.Mirror)
  {
    tile.Mirror->RemoveObservers(vtkCommand::ModifiedEvent, this->ContentModifiedCallbackCommand);
    tile.Mirror->Clear();
    tile.Mirror->SetSourceScene(nullptr);
  }
  if (tile.Renderer)
  {
    tile.Renderer->RemoveAllViewProps();
    vtkRenderWindow* renderWindow = tile.Renderer->GetRenderWindow();
    if (renderWindow)
    {
      for (vtkRenderer* overlayRenderer : tile.OverlayRenderers)
      {
        renderWindow->RemoveRenderer(overlayRenderer);
      }
      renderWindow->RemoveRenderer(tile.Renderer);
    }
  }
  tile = Tile();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::DestroyAllTiles()
{
  for (Tile& tile : this->Tiles)
  {
    this->DestroyTile(tile);
  }
  this->Tiles.clear();
  this->ReferenceViewNode = nullptr;
  this->BrowserNode = nullptr;
  this->RegisteredDisplayableManagerCount = -1;
  this->ActiveTileIndex = -1;
  this->HoveredTileIndex = -1;
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::UpdateTiles()
{
  if (this->ProcessingTileEvent)
  {
    this->TileUpdateDeferred = true;
    return;
  }
  if (this->UpdatingTiles)
  {
    return;
  }
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  vtkRenderer* mainRenderer = this->External->GetRenderer();
  if (!viewNode || !mainRenderer || !mainRenderer->GetRenderWindow())
  {
    this->DestroyAllTiles();
    return;
  }
  vtkMRMLAbstractViewNode* referenceViewNode = viewNode->GetReferenceViewNode();
  vtkMRMLSequenceBrowserNode* browserNode = viewNode->GetSequenceBrowserNode();
  if (!referenceViewNode || !browserNode)
  {
    this->DestroyAllTiles();
    return;
  }

  this->UpdatingTiles = true;

  // The tiles are built for a specific reference view, browser node and displayable manager
  // list. If any of them changed then they are rebuilt from scratch.
  vtkMRMLDisplayableManagerFactory* sourceFactory = this->GetSourceDisplayableManagerFactory(viewNode->IsThreeDMode());
  if (referenceViewNode != this->ReferenceViewNode.GetPointer() //
      || browserNode != this->BrowserNode.GetPointer()          //
      || sourceFactory->GetRegisteredDisplayableManagerCount() != this->RegisteredDisplayableManagerCount)
  {
    this->DestroyAllTiles();
    this->ReferenceViewNode = referenceViewNode;
    this->BrowserNode = browserNode;
    this->RegisteredDisplayableManagerCount = sourceFactory->GetRegisteredDisplayableManagerCount();
  }

  int numberOfTiles = viewNode->GetNumberOfTiles();

  // Remove extra tiles.
  while (static_cast<int>(this->Tiles.size()) > numberOfTiles)
  {
    this->DestroyTile(this->Tiles.back());
    this->Tiles.pop_back();
  }
  // Create missing tiles.
  while (static_cast<int>(this->Tiles.size()) < numberOfTiles)
  {
    this->Tiles.emplace_back();
    if (!this->CreateTile(this->Tiles.back(), referenceViewNode, browserNode))
    {
      this->DestroyTile(this->Tiles.back());
      this->Tiles.pop_back();
      break;
    }
  }

  this->UpdateTileLayout();
  this->UpdateTileContent();
  this->ContentUpdateRequested = false;

  this->UpdatingTiles = false;
  this->RequestRenderAfterUpdate();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::UpdateTileLayout()
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  vtkRenderer* mainRenderer = this->External->GetRenderer();
  vtkRenderWindow* renderWindow = mainRenderer ? mainRenderer->GetRenderWindow() : nullptr;
  if (!viewNode || !renderWindow || this->Tiles.empty())
  {
    return;
  }

  int rows = viewNode->GetRows();
  int columns = viewNode->GetColumns();
  const int* viewSize = renderWindow->GetSize();
  this->LastViewSize[0] = viewSize[0];
  this->LastViewSize[1] = viewSize[1];

  // The tiles are separated by gaps, in which the background of the view is visible. The gaps
  // are not wider than what leaves at least a few pixels for the tiles.
  int spacing = viewNode->GetTileSpacing();
  spacing = std::min(spacing, std::max(0, (viewSize[0] - 4 * columns) / std::max(1, columns - 1)));
  spacing = std::min(spacing, std::max(0, (viewSize[1] - 4 * rows) / std::max(1, rows - 1)));
  // Pixel size of a tile. All tiles have the same size (the slice image of every tile has the
  // same dimensions), the few pixels that are left over stay empty at the right and at the top.
  // A view that is smaller than the grid still gets valid (overlapping) viewports.
  int tileWidth = std::max(1, (viewSize[0] - spacing * (columns - 1)) / columns);
  int tileHeight = std::max(1, (viewSize[1] - spacing * (rows - 1)) / rows);

  for (int tileIndex = 0; tileIndex < static_cast<int>(this->Tiles.size()); ++tileIndex)
  {
    Tile& tile = this->Tiles[tileIndex];
    int row = tileIndex / columns;
    int column = tileIndex % columns;
    // The first tile is in the top-left corner. Viewports are normalized coordinates.
    int xMinPixel = column * (tileWidth + spacing);
    int yMinPixel = (rows - 1 - row) * (tileHeight + spacing);
    double viewport[4] = { 0.0, 0.0, 1.0, 1.0 };
    if (viewSize[0] > 0 && viewSize[1] > 0)
    {
      viewport[0] = static_cast<double>(xMinPixel) / viewSize[0];
      viewport[1] = static_cast<double>(yMinPixel) / viewSize[1];
      viewport[2] = static_cast<double>(xMinPixel + tileWidth) / viewSize[0];
      viewport[3] = static_cast<double>(yMinPixel + tileHeight) / viewSize[1];
    }
    if (tile.Renderer)
    {
      tile.Renderer->SetViewport(viewport);
    }
    for (vtkRenderer* overlayRenderer : tile.OverlayRenderers)
    {
      // Overlay renderers that their owner positioned since the last update (for example the
      // renderer of an orientation marker, which is placed in a corner of the view) are left alone.
      const double* currentViewport = overlayRenderer->GetViewport();
      if (std::equal(currentViewport, currentViewport + 4, tile.OverlayViewport))
      {
        overlayRenderer->SetViewport(viewport);
      }
    }
    std::copy(viewport, viewport + 4, tile.OverlayViewport);

    if (tile.SliceLogic && tileWidth > 0 && tileHeight > 0)
    {
      vtkMRMLSliceNode* tileSliceNode = tile.SliceLogic->GetSliceNode();
      if (tileSliceNode)
      {
        int dimensions[3] = { 0, 0, 0 };
        tileSliceNode->GetDimensions(dimensions);
        if (dimensions[0] != tileWidth || dimensions[1] != tileHeight)
        {
          tileSliceNode->SetDimensions(tileWidth, tileHeight, 1);
          // The field of view must be recomputed for the new aspect ratio. It is copied from
          // the reference view, so a full update of the mirror is requested.
          if (tile.Mirror)
          {
            tile.Mirror->Modified();
          }
        }
      }
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::UpdateTileContent()
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  if (!viewNode)
  {
    return;
  }
  vtkMRMLSequenceBrowserNode* browserNode = viewNode->GetSequenceBrowserNode();
  int selectedItemNumber = (browserNode ? browserNode->GetSelectedItemNumber() : -1);
  bool showItemLabels = viewNode->GetShowItemLabels();
  bool highlightSelectedItem = viewNode->GetHighlightSelectedItem();
  double layoutColor[3] = { 1.0, 1.0, 1.0 };
  viewNode->GetLayoutColor(layoutColor);

  if (this->ReuseTilesForItems())
  {
    // The viewports of the moved tiles must follow their new position.
    this->UpdateTileLayout();
  }

  for (int tileIndex = 0; tileIndex < static_cast<int>(this->Tiles.size()); ++tileIndex)
  {
    Tile& tile = this->Tiles[tileIndex];
    int itemNumber = viewNode->GetItemNumberForTile(tileIndex);
    tile.ItemNumber = itemNumber;

    if (tile.Mirror)
    {
      tile.Mirror->SetItemNumber(itemNumber);
      tile.Mirror->Update();
    }

    if (tile.Renderer)
    {
      // A tile that has no item to show is not drawn at all: the background of the view is
      // visible in its place, which makes it clear that the position is empty.
      bool draw = (itemNumber >= 0);
      tile.Renderer->SetDraw(draw);
      for (vtkRenderer* overlayRenderer : tile.OverlayRenderers)
      {
        overlayRenderer->SetDraw(draw);
      }
    }

    if (tile.SliceLogic && tile.ImageMapper && tile.ImageActor)
    {
      vtkAlgorithmOutput* imageDataConnection = tile.SliceLogic->GetImageDataConnection();
      if (tile.ImageMapper->GetNumberOfInputConnections(0) == 0 //
          || tile.ImageMapper->GetInputConnection(0, 0) != imageDataConnection)
      {
        tile.ImageMapper->SetInputConnection(imageDataConnection);
      }
      tile.ImageActor->SetVisibility(imageDataConnection != nullptr && itemNumber >= 0);
    }

    if (tile.Renderer && !tile.SliceLogic)
    {
      // 3D views: the camera of the tile is a mirror of the camera of the reference view,
      // but the clipping range depends on the content of this tile.
      if (tile.Renderer->IsActiveCameraCreated())
      {
        tile.Renderer->ResetCameraClippingRange();
      }
    }

    if (tile.LabelActor)
    {
      std::string label;
      if (showItemLabels && itemNumber >= 0 && browserNode)
      {
        if (browserNode->GetIndexDisplayMode() == vtkMRMLSequenceBrowserNode::IndexDisplayAsIndexValue)
        {
          label = browserNode->GetFormattedIndexValue(itemNumber);
        }
        else
        {
          std::stringstream labelStream;
          labelStream << itemNumber + 1 << "/" << browserNode->GetNumberOfItems();
          label = labelStream.str();
        }
      }
      tile.LabelActor->SetInput(label.c_str());
      tile.LabelActor->SetVisibility(!label.empty());
    }

    if (tile.HighlightActor)
    {
      bool highlighted = highlightSelectedItem && itemNumber >= 0 && itemNumber == selectedItemNumber;
      tile.HighlightActor->GetProperty()->SetColor(layoutColor);
      tile.HighlightActor->SetVisibility(highlighted);
    }
  }
  this->UpdateHoverFrames();
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::ReuseTilesForItems()
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  int numberOfTiles = static_cast<int>(this->Tiles.size());
  if (!viewNode || numberOfTiles < 2)
  {
    return false;
  }

  // For each position, the tile (identified by its current index) that will be displayed there.
  std::vector<int> sourceTileIndices(numberOfTiles, -1);
  std::vector<bool> tileUsed(numberOfTiles, false);
  bool identity = true;
  // First the tiles that already show the item that is needed at a position (empty tiles are not
  // reused this way, they have nothing to preserve).
  for (int tileIndex = 0; tileIndex < numberOfTiles; ++tileIndex)
  {
    int itemNumber = viewNode->GetItemNumberForTile(tileIndex);
    if (itemNumber < 0)
    {
      continue;
    }
    if (this->Tiles[tileIndex].ItemNumber == itemNumber)
    {
      // Already in place (preferred over an identical item shown in another tile).
      sourceTileIndices[tileIndex] = tileIndex;
      tileUsed[tileIndex] = true;
    }
  }
  for (int tileIndex = 0; tileIndex < numberOfTiles; ++tileIndex)
  {
    if (sourceTileIndices[tileIndex] >= 0)
    {
      continue;
    }
    int itemNumber = viewNode->GetItemNumberForTile(tileIndex);
    if (itemNumber < 0)
    {
      continue;
    }
    for (int sourceTileIndex = 0; sourceTileIndex < numberOfTiles; ++sourceTileIndex)
    {
      if (!tileUsed[sourceTileIndex] && this->Tiles[sourceTileIndex].ItemNumber == itemNumber)
      {
        sourceTileIndices[tileIndex] = sourceTileIndex;
        tileUsed[sourceTileIndex] = true;
        identity = false;
        break;
      }
    }
  }
  // Then the remaining positions get the remaining tiles, keeping them in place where possible.
  for (int tileIndex = 0; tileIndex < numberOfTiles; ++tileIndex)
  {
    if (sourceTileIndices[tileIndex] >= 0)
    {
      continue;
    }
    int sourceTileIndex = tileIndex;
    if (tileUsed[sourceTileIndex])
    {
      for (sourceTileIndex = 0; sourceTileIndex < numberOfTiles && tileUsed[sourceTileIndex]; ++sourceTileIndex)
      {
      }
      identity = false;
    }
    sourceTileIndices[tileIndex] = sourceTileIndex;
    tileUsed[sourceTileIndex] = true;
  }
  if (identity)
  {
    return false;
  }

  std::vector<Tile> reorderedTiles(numberOfTiles);
  std::vector<int> newTileIndices(numberOfTiles, -1);
  for (int tileIndex = 0; tileIndex < numberOfTiles; ++tileIndex)
  {
    reorderedTiles[tileIndex] = std::move(this->Tiles[sourceTileIndices[tileIndex]]);
    newTileIndices[sourceTileIndices[tileIndex]] = tileIndex;
  }
  this->Tiles.swap(reorderedTiles);
  // The tiles that are being interacted with are identified by their position.
  if (this->ActiveTileIndex >= 0 && this->ActiveTileIndex < numberOfTiles)
  {
    this->ActiveTileIndex = newTileIndices[this->ActiveTileIndex];
  }
  if (this->HoveredTileIndex >= 0 && this->HoveredTileIndex < numberOfTiles)
  {
    this->HoveredTileIndex = newTileIndices[this->HoveredTileIndex];
  }
  return true;
}

//---------------------------------------------------------------------------
int vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::GetTileAtDisplayPosition(int x, int y)
{
  for (int tileIndex = 0; tileIndex < static_cast<int>(this->Tiles.size()); ++tileIndex)
  {
    vtkRenderer* renderer = this->Tiles[tileIndex].Renderer;
    if (renderer && renderer->IsInViewport(x, y))
    {
      return tileIndex;
    }
  }
  return -1;
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::IsTileInteractionEnabled()
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  return viewNode != nullptr && viewNode->GetInteractiveTiles() && !this->Tiles.empty();
}

//---------------------------------------------------------------------------
int vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::GetTargetTileIndex(vtkMRMLInteractionEventData* eventData)
{
  int numberOfTiles = static_cast<int>(this->Tiles.size());
  if (!eventData || numberOfTiles == 0)
  {
    return -1;
  }
  if (eventData->GetType() == vtkCommand::LeaveEvent)
  {
    // The pointer left the view: the tile that it was over has to be notified.
    return (this->HoveredTileIndex < numberOfTiles ? this->HoveredTileIndex : -1);
  }
  if (this->ActiveTileIndex >= 0 && this->ActiveTileIndex < numberOfTiles)
  {
    // A mouse button is pressed: the tile where it was pressed receives the whole drag.
    return this->ActiveTileIndex;
  }
  if (eventData->IsDisplayPositionValid())
  {
    const int* displayPosition = eventData->GetDisplayPosition();
    return this->GetTileAtDisplayPosition(displayPosition[0], displayPosition[1]);
  }
  // Events without a position (for example 3D controller events) go to the tile under the pointer.
  return (this->HoveredTileIndex < numberOfTiles ? this->HoveredTileIndex : -1);
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::SetHoveredTileIndex(int tileIndex, vtkMRMLInteractionEventData* eventData)
{
  if (tileIndex == this->HoveredTileIndex)
  {
    return;
  }
  if (this->HoveredTileIndex >= 0 && this->HoveredTileIndex < static_cast<int>(this->Tiles.size()))
  {
    // The widgets of the previously hovered tile must not keep their hover highlight (the
    // pointer will not move over them again, so they would not notice on their own).
    Tile& previousTile = this->Tiles[this->HoveredTileIndex];
    if (previousTile.FocusedDisplayableManager)
    {
      previousTile.FocusedDisplayableManager->SetHasFocus(false, eventData);
      previousTile.FocusedDisplayableManager = nullptr;
    }
  }
  this->HoveredTileIndex = tileIndex;
  this->UpdateHoverFrames();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::UpdateHoverFrames()
{
  bool interactive = this->IsTileInteractionEnabled();
  bool changed = false;
  for (int tileIndex = 0; tileIndex < static_cast<int>(this->Tiles.size()); ++tileIndex)
  {
    Tile& tile = this->Tiles[tileIndex];
    if (!tile.HoverFrameActor)
    {
      continue;
    }
    bool visible = (interactive && tileIndex == this->HoveredTileIndex && tile.ItemNumber >= 0);
    if (static_cast<bool>(tile.HoverFrameActor->GetVisibility()) != visible)
    {
      tile.HoverFrameActor->SetVisibility(visible);
      changed = true;
    }
  }
  if (changed)
  {
    this->RequestRender();
  }
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::ProcessKeyPressEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!eventData || eventData->GetType() != vtkCommand::KeyPressEvent)
  {
    return false;
  }
  vtkMRMLSequenceLightboxViewNode* viewNode = this->GetViewNode();
  vtkMRMLSequenceBrowserNode* browserNode = (viewNode ? viewNode->GetSequenceBrowserNode() : nullptr);
  if (!viewNode || !browserNode || browserNode->GetNumberOfItems() <= 0)
  {
    return false;
  }
  // The arrow keys and the Home/End/Page keys are used by the views themselves (slice views scroll
  // the slice, 3D views rotate the camera), so the items are stepped through with the Shift
  // modifier, like with the mouse wheel.
  if (!(eventData->GetModifiers() & vtkEvent::ShiftModifier))
  {
    return false;
  }
  std::string key = eventData->GetKeySym();
  if (key.empty())
  {
    return false;
  }
  int numberOfItems = browserNode->GetNumberOfItems();
  int rowStep = viewNode->GetColumns() * viewNode->GetItemStep();
  bool fixedGrid = (viewNode->GetItemSelectionMode() == vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed);
  if (key == "Left" || key == "Right")
  {
    // Previous or next item.
    browserNode->SelectNextItem(key == "Left" ? -1 : 1);
    return true;
  }
  if (key == "Prior" || key == "Next")
  {
    // Page Up/Down: previous or next row of tiles (the grid is scrolled if it does not follow
    // the selected item).
    int increment = (key == "Prior" ? -rowStep : rowStep);
    if (fixedGrid)
    {
      int startItemNumber = std::max(0, std::min(numberOfItems - 1, viewNode->GetStartItemNumber() + increment));
      viewNode->SetStartItemNumber(startItemNumber);
    }
    else
    {
      browserNode->SelectNextItem(increment);
    }
    return true;
  }
  if (key == "Home" || key == "End")
  {
    // First or last item.
    if (fixedGrid)
    {
      viewNode->SetStartItemNumber(key == "Home" ? 0 : std::max(0, numberOfItems - viewNode->GetNumberOfTiles() * viewNode->GetItemStep()));
    }
    browserNode->SetSelectedItemNumber(key == "Home" ? 0 : numberOfItems - 1);
    return true;
  }
  return false;
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::SetPressPosition(vtkMRMLInteractionEventData* eventData)
{
  this->PressDisplayPositionValid = (eventData != nullptr && eventData->IsDisplayPositionValid());
  if (this->PressDisplayPositionValid)
  {
    const int* displayPosition = eventData->GetDisplayPosition();
    this->PressDisplayPosition[0] = displayPosition[0];
    this->PressDisplayPosition[1] = displayPosition[1];
  }
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::IsClick(vtkMRMLInteractionEventData* eventData)
{
  if (!eventData)
  {
    return false;
  }
  if (!this->PressDisplayPositionValid || !eventData->IsDisplayPositionValid())
  {
    // Events without a position (for example 3D controller events): any movement is a drag.
    return !eventData->GetMouseMovedSinceButtonDown();
  }
  const int* displayPosition = eventData->GetDisplayPosition();
  int dx = displayPosition[0] - this->PressDisplayPosition[0];
  int dy = displayPosition[1] - this->PressDisplayPosition[1];
  return dx * dx + dy * dy <= ClickTolerance * ClickTolerance;
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::CreateTileEventData(Tile& tile, vtkMRMLInteractionEventData* eventData, vtkMRMLInteractionEventData* tileEventData)
{
  tileEventData->SetType(eventData->GetType());
  tileEventData->SetModifiers(eventData->GetModifiers());
  tileEventData->SetKeyCode(eventData->GetKeyCode());
  tileEventData->SetKeyRepeatCount(eventData->GetKeyRepeatCount());
  tileEventData->SetKeySym(eventData->GetKeySym());
  tileEventData->SetMouseMovedSinceButtonDown(eventData->GetMouseMovedSinceButtonDown());
  tileEventData->SetRotation(eventData->GetRotation());
  tileEventData->SetLastRotation(eventData->GetLastRotation());
  tileEventData->SetScale(eventData->GetScale());
  tileEventData->SetLastScale(eventData->GetLastScale());
  tileEventData->SetTranslation(eventData->GetTranslation());
  tileEventData->SetLastTranslation(eventData->GetLastTranslation());
  tileEventData->SetDevice(eventData->GetDevice());
  tileEventData->SetInput(eventData->GetInput());
  tileEventData->SetAction(eventData->GetAction());
  tileEventData->SetInteractionContextName(eventData->GetInteractionContextName());

  if (!eventData->IsDisplayPositionValid() || !tile.Renderer)
  {
    tileEventData->SetDisplayPositionInvalid();
    return;
  }

  // The position of the event is relative to the renderer of the lightbox view, which fills the
  // whole render window, so it is a position in the render window.
  const int* displayPosition = eventData->GetDisplayPosition();
  vtkMRMLSliceNode* tileSliceNode = (tile.SliceLogic ? tile.SliceLogic->GetSliceNode() : nullptr);
  if (tileSliceNode)
  {
    // Slice views work in XY coordinates: pixel coordinates relative to the origin of the
    // view, which is the origin of the viewport of the tile. The XY to RAS transform of the
    // slice node gives the world position (as in vtkMRMLSliceViewInteractorStyle).
    const int* tileOrigin = tile.Renderer->GetOrigin();
    int tileDisplayPosition[2] = { displayPosition[0] - tileOrigin[0], displayPosition[1] - tileOrigin[1] };
    tileEventData->SetDisplayPosition(tileDisplayPosition);
    double xyz[4] = { static_cast<double>(tileDisplayPosition[0]), static_cast<double>(tileDisplayPosition[1]), 0.0, 1.0 };
    double ras[4] = { 0.0, 0.0, 0.0, 1.0 };
    tileSliceNode->GetXYToRAS()->MultiplyPoint(xyz, ras);
    tileEventData->SetWorldPosition(ras);
    return;
  }

  // 3D views: the widget representations convert world positions to display positions using the
  // renderer of the tile, which returns render window coordinates (the viewport offset is
  // included), so the event position is passed on as is (as in vtkMRMLThreeDViewInteractorStyle).
  tileEventData->SetDisplayPosition(displayPosition);
  tileEventData->SetRenderer(tile.Renderer);
  tileEventData->SetAccuratePicker(this->AccuratePicker);
  if (eventData->GetType() != vtkCommand::ConfigureEvent)
  {
    // Fast, approximate world position from the depth buffer. Widgets that need an accurate
    // position compute it on demand using the accurate picker.
    double worldPosition[3] = { 0.0, 0.0, 0.0 };
    if (this->QuickPicker->Pick(displayPosition[0], displayPosition[1], 0, tile.Renderer))
    {
      this->QuickPicker->GetPickPosition(worldPosition);
      tileEventData->SetWorldPosition(worldPosition, false);
    }
  }
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::ProcessTileEventData(Tile& tile, vtkMRMLInteractionEventData* tileEventData)
{
  // Same logic as vtkMRMLViewInteractorStyle::DelegateInteractionEventDataToDisplayableManagers,
  // applied to the displayable managers of the tile.
  if (!tile.Group)
  {
    return false;
  }
  if (tileEventData->GetType() == vtkCommand::Button3DEvent || tileEventData->GetType() == vtkCommand::Move3DEvent)
  {
    tileEventData->SetDisplayPositionInvalid();
  }

  vtkMRMLAbstractDisplayableManager* closestDisplayableManager = nullptr;
  double closestDistance2 = VTK_DOUBLE_MAX;
  int numberOfDisplayableManagers = tile.Group->GetDisplayableManagerCount();
  for (int displayableManagerIndex = 0; displayableManagerIndex < numberOfDisplayableManagers; ++displayableManagerIndex)
  {
    vtkMRMLAbstractDisplayableManager* displayableManager = tile.Group->GetNthDisplayableManager(displayableManagerIndex);
    if (!displayableManager)
    {
      continue;
    }
    double distance2 = VTK_DOUBLE_MAX;
    if (displayableManager->CanProcessInteractionEvent(tileEventData, distance2) //
        && (!closestDisplayableManager || distance2 < closestDistance2))
    {
      closestDisplayableManager = displayableManager;
      closestDistance2 = distance2;
    }
  }

  if (!closestDisplayableManager)
  {
    if (tileEventData->GetType() != vtkCommand::KeyPressEvent && tileEventData->GetType() != vtkCommand::KeyReleaseEvent)
    {
      this->External->SetMouseCursor(VTK_CURSOR_DEFAULT);
    }
    return false;
  }

  vtkMRMLAbstractDisplayableManager* previousDisplayableManager = tile.FocusedDisplayableManager;
  if (previousDisplayableManager != closestDisplayableManager)
  {
    if (previousDisplayableManager)
    {
      previousDisplayableManager->SetHasFocus(false, tileEventData);
    }
    tile.FocusedDisplayableManager = closestDisplayableManager;
    closestDisplayableManager->SetHasFocus(true, tileEventData);
  }

  // The displayable managers of the tile modify the nodes of the tile scene (slice position,
  // camera, markup positions, ...). While the event is processed these modifications are written
  // back to the scene of the application, so that the interaction has the same effect as in a
  // regular view. Write-back is disabled otherwise, so that the rendering pipeline of the tile
  // (which also modifies nodes of the tile scene) does not modify the application scene.
  if (tile.Mirror)
  {
    tile.Mirror->SetWriteBackEnabled(true);
  }
  bool processed = closestDisplayableManager->ProcessInteractionEvent(tileEventData);
  if (tile.Mirror)
  {
    // Writes back the pending modifications.
    tile.Mirror->SetWriteBackEnabled(false);
  }
  this->External->SetMouseCursor(processed ? closestDisplayableManager->GetMouseCursor() : VTK_CURSOR_DEFAULT);
  return processed;
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::vtkInternal::DelegateInteractionEventToTile(int tileIndex, vtkMRMLInteractionEventData* eventData)
{
  if (tileIndex < 0 || tileIndex >= static_cast<int>(this->Tiles.size()) || !eventData)
  {
    return false;
  }
  Tile& tile = this->Tiles[tileIndex];
  if (!tile.Renderer || !tile.Group)
  {
    return false;
  }

  vtkMRMLSliceNode* tileSliceNode = (tile.SliceLogic ? tile.SliceLogic->GetSliceNode() : nullptr);
  vtkNew<vtkMatrix4x4> xyToRasBefore;
  if (tileSliceNode)
  {
    xyToRasBefore->DeepCopy(tileSliceNode->GetXYToRAS());
  }

  bool wasProcessingTileEvent = this->ProcessingTileEvent;
  this->ProcessingTileEvent = true;

  vtkNew<vtkMRMLInteractionEventData> tileEventData;
  this->CreateTileEventData(tile, eventData, tileEventData);
  bool processed = this->ProcessTileEventData(tile, tileEventData);

  // If the slice of the tile moved or zoomed while the event was processed then the widgets of the
  // tile are notified about the new position under the pointer (for example a markup that is
  // being placed follows the slice), as vtkMRMLSliceViewInteractorStyle does.
  if (tileSliceNode && eventData->IsDisplayPositionValid())
  {
    bool xyToRasChanged = false;
    vtkMatrix4x4* xyToRasAfter = tileSliceNode->GetXYToRAS();
    for (int row = 0; row < 4 && !xyToRasChanged; ++row)
    {
      for (int column = 0; column < 4; ++column)
      {
        if (xyToRasBefore->GetElement(row, column) != xyToRasAfter->GetElement(row, column))
        {
          xyToRasChanged = true;
          break;
        }
      }
    }
    if (xyToRasChanged)
    {
      vtkNew<vtkMRMLInteractionEventData> mouseMoveEventData;
      this->CreateTileEventData(tile, eventData, mouseMoveEventData);
      mouseMoveEventData->SetType(vtkCommand::MouseMoveEvent);
      this->ProcessTileEventData(tile, mouseMoveEventData);
    }
  }

  this->ProcessingTileEvent = wasProcessingTileEvent;
  if (!this->ProcessingTileEvent && this->TileUpdateDeferred)
  {
    this->TileUpdateDeferred = false;
    this->UpdateTiles();
  }
  return processed;
}

//---------------------------------------------------------------------------
// vtkMRMLSequenceLightboxDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLSequenceLightboxDisplayableManager::vtkMRMLSequenceLightboxDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLSequenceLightboxDisplayableManager::~vtkMRMLSequenceLightboxDisplayableManager()
{
  delete this->Internal;
  this->Internal = nullptr;
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Number of tiles: " << this->GetNumberOfTiles() << "\n";
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::AdditionalInitializeStep()
{
  // The lightbox view node notifies about changes of the browser node and of the reference
  // view node using its own events.
  this->AddMRMLDisplayableManagerEvent(vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeModifiedEvent);
  this->AddMRMLDisplayableManagerEvent(vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent);
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::Create()
{
  this->Internal->AddRendererUpdateObserver(this->GetRenderer());
  this->SetUpdateFromMRMLRequested(true);
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::SetMRMLSceneInternal(vtkMRMLScene* newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::StartCloseEvent);
  events->InsertNextValue(vtkMRMLScene::EndCloseEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneStartClose()
{
  // The mirrors keep a reference to the nodes of the scene, they must be released before the
  // scene is cleared.
  this->Internal->DestroyAllTiles();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneEndClose()
{
  this->Internal->DestroyAllTiles();
  this->SetUpdateFromMRMLRequested(true);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneEndBatchProcess()
{
  this->SetUpdateFromMRMLRequested(true);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneEndImport()
{
  this->SetUpdateFromMRMLRequested(true);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (this->GetMRMLScene() && this->GetMRMLScene()->IsBatchProcessing())
  {
    // Updated once, when the batch processing ends.
    return;
  }
  if (node                                        //
      && (node->IsA("vtkMRMLSequenceBrowserNode") //
          || node->IsA("vtkMRMLAbstractViewNode")))
  {
    // The browser node or the reference view may have become available.
    this->Internal->RequestTileUpdate();
  }
  // Other nodes are handled by the scene mirrors of the tiles, which observe the scene.
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if (node                                        //
      && (node->IsA("vtkMRMLSequenceBrowserNode") //
          || node->IsA("vtkMRMLAbstractViewNode")))
  {
    // The reference view or the browser node may have been removed.
    this->Internal->DestroyAllTiles();
    if (!this->GetMRMLScene() || !this->GetMRMLScene()->IsBatchProcessing())
    {
      this->SetUpdateFromMRMLRequested(true);
      this->RequestRender();
    }
  }
  // Other nodes are handled by the scene mirrors of the tiles, which observe the scene.
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  if (caller == this->GetMRMLDisplayableNode()                                       //
      && (event == vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeModifiedEvent //
          || event == vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent))
  {
    // The browser node or the reference view has been replaced or modified (for example another
    // item was selected, or the slice was scrolled).
    this->Internal->RequestTileUpdate();
    return;
  }
  this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  this->SetUpdateFromMRMLRequested(true);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::UpdateFromMRML()
{
  // RequestRender() does not reset the flag, it must be done here to avoid endless updates.
  this->SetUpdateFromMRMLRequested(false);
  this->Internal->UpdateTiles();
}

//---------------------------------------------------------------------------
void vtkMRMLSequenceLightboxDisplayableManager::SynchronizeTiles()
{
  // The view is rendered right after this: the tiles do not need to request a render (which
  // could render the view immediately, before the synchronization is complete).
  bool wasSuppressed = this->Internal->SuppressRenderRequests;
  this->Internal->SuppressRenderRequests = true;
  this->Internal->UpdateTileContentIfRequested();
  this->Internal->SuppressRenderRequests = wasSuppressed;
}

//---------------------------------------------------------------------------
int vtkMRMLSequenceLightboxDisplayableManager::GetNumberOfTiles()
{
  return static_cast<int>(this->Internal->Tiles.size());
}

//---------------------------------------------------------------------------
vtkRenderer* vtkMRMLSequenceLightboxDisplayableManager::GetTileRenderer(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return nullptr;
  }
  return this->Internal->Tiles[tileIndex].Renderer;
}

//---------------------------------------------------------------------------
vtkMRMLScene* vtkMRMLSequenceLightboxDisplayableManager::GetTileScene(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return nullptr;
  }
  vtkSequenceLightboxSceneMirror* mirror = this->Internal->Tiles[tileIndex].Mirror;
  return mirror ? mirror->GetTileScene() : nullptr;
}

//---------------------------------------------------------------------------
vtkSequenceLightboxSceneMirror* vtkMRMLSequenceLightboxDisplayableManager::GetTileMirror(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return nullptr;
  }
  return this->Internal->Tiles[tileIndex].Mirror;
}

//---------------------------------------------------------------------------
vtkMRMLDisplayableManagerGroup* vtkMRMLSequenceLightboxDisplayableManager::GetTileDisplayableManagerGroup(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return nullptr;
  }
  return this->Internal->Tiles[tileIndex].Group;
}

//---------------------------------------------------------------------------
int vtkMRMLSequenceLightboxDisplayableManager::GetTileItemNumber(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return -1;
  }
  return this->Internal->Tiles[tileIndex].ItemNumber;
}

//---------------------------------------------------------------------------
int vtkMRMLSequenceLightboxDisplayableManager::GetTileAtDisplayPosition(int x, int y)
{
  return this->Internal->GetTileAtDisplayPosition(x, y);
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2)
{
  if (!eventData)
  {
    return false;
  }
  vtkMRMLSequenceLightboxViewNode* viewNode = this->Internal->GetViewNode();
  if (!viewNode || this->GetNumberOfTiles() == 0)
  {
    return false;
  }
  // The tiles must be up to date when they process an event: the event may arrive before the
  // view is rendered again (for example when events are sent by a script), while the tiles are
  // normally synchronized right before a render. Nothing is done if they are up to date.
  this->Internal->UpdateTileContentIfRequested();

  if (this->Internal->IsTileInteractionEnabled())
  {
    // Every event that concerns a tile is forwarded to the displayable managers of that tile.
    if (this->Internal->GetTargetTileIndex(eventData) < 0)
    {
      return false;
    }
    distance2 = 0.0;
    return true;
  }

  // Tiles are not interactive: a click selects the item of the tile, the mouse wheel scrolls
  // the grid and the keyboard steps through the items.
  unsigned long eventType = eventData->GetType();
  if (eventType == vtkCommand::KeyPressEvent)
  {
    distance2 = 0.0;
    return true;
  }
  if (eventType != vtkCommand::LeftButtonPressEvent      //
      && eventType != vtkCommand::LeftButtonReleaseEvent //
      && eventType != vtkCommand::MouseWheelForwardEvent //
      && eventType != vtkCommand::MouseWheelBackwardEvent)
  {
    return false;
  }
  if (!eventData->IsDisplayPositionValid())
  {
    return false;
  }
  const int* displayPosition = eventData->GetDisplayPosition();
  if (this->Internal->GetTileAtDisplayPosition(displayPosition[0], displayPosition[1]) < 0)
  {
    return false;
  }
  distance2 = 0.0;
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!eventData)
  {
    return false;
  }
  vtkMRMLSequenceLightboxViewNode* viewNode = this->Internal->GetViewNode();
  if (!viewNode)
  {
    return false;
  }
  vtkMRMLSequenceBrowserNode* browserNode = viewNode->GetSequenceBrowserNode();
  if (!browserNode)
  {
    return false;
  }
  unsigned long eventType = eventData->GetType();

  if (this->Internal->IsTileInteractionEnabled())
  {
    int tileIndex = this->Internal->GetTargetTileIndex(eventData);
    if (tileIndex < 0)
    {
      return false;
    }

    bool buttonPress = (eventType == vtkCommand::LeftButtonPressEvent      //
                        || eventType == vtkCommand::MiddleButtonPressEvent //
                        || eventType == vtkCommand::RightButtonPressEvent);
    bool buttonRelease = (eventType == vtkCommand::LeftButtonReleaseEvent      //
                          || eventType == vtkCommand::MiddleButtonReleaseEvent //
                          || eventType == vtkCommand::RightButtonReleaseEvent);
    if (buttonPress && this->Internal->ActiveTileIndex < 0)
    {
      // The tile receives all events until the button is released, even if the pointer leaves it.
      this->Internal->ActiveTileIndex = tileIndex;
      this->Internal->ActiveTilePressProcessed = false;
      this->Internal->SetPressPosition(eventData);
    }

    if (eventType == vtkCommand::LeaveEvent)
    {
      this->Internal->DelegateInteractionEventToTile(tileIndex, eventData);
      this->Internal->SetHoveredTileIndex(-1, eventData);
      return true;
    }
    this->Internal->SetHoveredTileIndex(tileIndex, eventData);

    bool processed = this->Internal->DelegateInteractionEventToTile(tileIndex, eventData);

    if (!processed && (eventType == vtkCommand::MouseWheelForwardEvent || eventType == vtkCommand::MouseWheelBackwardEvent))
    {
      // The tiles use the mouse wheel for scrolling the slice (or zooming the 3D view), so the
      // displayed items are scrolled with a modifier that the tiles do not use (Shift).
      processed = this->ProcessMouseWheelEvent(eventData);
    }
    if (!processed && eventType == vtkCommand::KeyPressEvent)
    {
      // Keys that the tile did not use step through the items.
      processed = this->Internal->ProcessKeyPressEvent(eventData);
    }

    if (buttonPress && this->Internal->ActiveTileIndex == tileIndex)
    {
      this->Internal->ActiveTilePressProcessed = this->Internal->ActiveTilePressProcessed || processed;
    }
    bool selectItem = false;
    if (buttonRelease)
    {
      // A click that no widget of the tile was interested in selects the item of the tile, as
      // in the non-interactive mode. Clicks that placed a markup or started a drag do not change
      // the selection, so that several items can be edited one after the other.
      selectItem = (eventType == vtkCommand::LeftButtonReleaseEvent && !processed //
                    && !this->Internal->ActiveTilePressProcessed                  //
                    && this->Internal->IsClick(eventData));
      this->Internal->ActiveTileIndex = -1;
      this->Internal->ActiveTilePressProcessed = false;
      this->Internal->PressDisplayPositionValid = false;
    }
    if (selectItem)
    {
      int itemNumber = this->GetTileItemNumber(tileIndex);
      if (itemNumber >= 0)
      {
        browserNode->SetSelectedItemNumber(itemNumber);
        processed = true;
      }
    }
    return processed;
  }

  if (eventType == vtkCommand::KeyPressEvent)
  {
    return this->Internal->ProcessKeyPressEvent(eventData);
  }
  if (!eventData->IsDisplayPositionValid())
  {
    return false;
  }
  const int* displayPosition = eventData->GetDisplayPosition();
  int tileIndex = this->Internal->GetTileAtDisplayPosition(displayPosition[0], displayPosition[1]);
  if (tileIndex < 0)
  {
    return false;
  }

  if (eventType == vtkCommand::LeftButtonPressEvent)
  {
    this->Internal->SetPressPosition(eventData);
    return true;
  }
  if (eventType == vtkCommand::LeftButtonReleaseEvent)
  {
    bool click = this->Internal->IsClick(eventData);
    this->Internal->PressDisplayPositionValid = false;
    if (!click)
    {
      // A drag, not a click.
      return false;
    }
    int itemNumber = this->GetTileItemNumber(tileIndex);
    if (itemNumber < 0)
    {
      return false;
    }
    // Selecting an item in the lightbox makes it the current item of the browser, which
    // updates the proxy nodes and therefore all the regular views as well.
    browserNode->SetSelectedItemNumber(itemNumber);
    return true;
  }

  return this->ProcessMouseWheelEvent(eventData);
}

//---------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxDisplayableManager::ProcessMouseWheelEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!eventData)
  {
    return false;
  }
  unsigned long eventType = eventData->GetType();
  if (eventType != vtkCommand::MouseWheelForwardEvent && eventType != vtkCommand::MouseWheelBackwardEvent)
  {
    return false;
  }
  vtkMRMLSequenceLightboxViewNode* viewNode = this->Internal->GetViewNode();
  vtkMRMLSequenceBrowserNode* browserNode = (viewNode ? viewNode->GetSequenceBrowserNode() : nullptr);
  if (!viewNode || !browserNode)
  {
    return false;
  }
  bool forward = (eventType == vtkCommand::MouseWheelForwardEvent);
  if (viewNode->GetItemSelectionMode() == vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed)
  {
    // Scroll the grid by one row.
    int increment = viewNode->GetColumns() * viewNode->GetItemStep();
    int startItemNumber = viewNode->GetStartItemNumber() + (forward ? -increment : increment);
    int numberOfItems = browserNode->GetNumberOfItems();
    if (startItemNumber < 0)
    {
      startItemNumber = 0;
    }
    if (numberOfItems > 0 && startItemNumber > numberOfItems - 1)
    {
      startItemNumber = numberOfItems - 1;
    }
    viewNode->SetStartItemNumber(startItemNumber);
    return true;
  }
  // In the other modes the displayed items are determined by the selected item of the browser
  // node (or by an explicit list of items, in which case stepping through the items is still the
  // most useful thing the wheel can do).
  browserNode->SelectNextItem(forward ? -1 : 1);
  return true;
}
