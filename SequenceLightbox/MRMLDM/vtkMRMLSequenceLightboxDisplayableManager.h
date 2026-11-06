/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __vtkMRMLSequenceLightboxDisplayableManager_h
#define __vtkMRMLSequenceLightboxDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractDisplayableManager.h"

#include "vtkSlicerSequenceLightboxModuleMRMLDisplayableManagerExport.h"

class vtkMRMLDisplayableManagerGroup;
class vtkMRMLScene;
class vtkRenderer;
class vtkSequenceLightboxSceneMirror;

/// \brief Displayable manager that renders a grid of sequence items ("lightbox").
///
/// The displayable manager splits the view into a grid of tiles. Each tile is a complete,
/// independent view: it has its own renderer (a viewport of the lightbox view's render
/// window), its own private MRML scene that mirrors the relevant part of the main scene
/// (see vtkSequenceLightboxSceneMirror), its own application logic and its own group of
/// displayable managers.
///
/// The tiles are therefore rendered by the standard slice view or 3D view displayable
/// managers - including the ones registered by modules, such as segmentations, markups or
/// volume rendering - and no rendering code is duplicated here. The only content that this
/// class renders itself is the item label and the highlight border of each tile.
///
/// Tiles follow the reference view of the lightbox view node: changing the slice position,
/// zoom or window/level, or moving the 3D camera updates every tile.
class VTK_SLICER_SEQUENCELIGHTBOX_MODULE_MRMLDISPLAYABLEMANAGER_EXPORT vtkMRMLSequenceLightboxDisplayableManager : public vtkMRMLAbstractDisplayableManager
{
public:
  static vtkMRMLSequenceLightboxDisplayableManager* New();
  vtkTypeMacro(vtkMRMLSequenceLightboxDisplayableManager, vtkMRMLAbstractDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Synchronize the content of the tiles with the scene, if anything that they display has
  /// changed since the last synchronization. Changes are collected and applied together, so this
  /// must be called right before the view is rendered (the view does that); nothing is done if
  /// the tiles are up to date. It must not be called while the view is being rendered: some
  /// displayable managers ignore the modifications made during rendering.
  void SynchronizeTiles();
  /// Number of tiles that are currently created.
  int GetNumberOfTiles();

  /// Renderer of the specified tile. Intended for testing.
  vtkRenderer* GetTileRenderer(int tileIndex);

  /// Mirror scene that is rendered in the specified tile. Intended for testing.
  vtkMRMLScene* GetTileScene(int tileIndex);

  /// Scene mirror that maintains the mirror scene of the specified tile. Intended for testing.
  vtkSequenceLightboxSceneMirror* GetTileMirror(int tileIndex);

  /// Displayable managers that render the content of the specified tile. Intended for testing.
  vtkMRMLDisplayableManagerGroup* GetTileDisplayableManagerGroup(int tileIndex);

  /// Sequence item number that is displayed in the specified tile, or -1 if the tile does not
  /// show a valid item. The tiles are synchronized with the scene before each render (see
  /// SynchronizeTiles), so this is the item that was displayed at the last render.
  int GetTileItemNumber(int tileIndex);

  /// Returns the index of the tile that contains the specified position in the view,
  /// or -1 if the position is outside all tiles.
  int GetTileAtDisplayPosition(int x, int y);

  bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2) override;
  bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

protected:
  vtkMRMLSequenceLightboxDisplayableManager();
  ~vtkMRMLSequenceLightboxDisplayableManager() override;

  /// Initialization that requires the renderer to be available.
  void AdditionalInitializeStep() override;

  void Create() override;

  /// Called from RequestRender method if UpdateFromMRMLRequested is true.
  void UpdateFromMRML() override;

  void SetMRMLSceneInternal(vtkMRMLScene* newScene) override;
  void OnMRMLSceneStartClose() override;
  void OnMRMLSceneEndClose() override;
  void OnMRMLSceneEndBatchProcess() override;
  void OnMRMLSceneEndImport() override;
  void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;
  void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

  /// Called when the lightbox view node is modified.
  void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller) override;

  /// Scroll the displayed items with the mouse wheel: in the fixed item selection mode the grid
  /// is scrolled by one row, in the other modes the selected item of the browser node is changed.
  /// Returns true if the event was a mouse wheel event.
  bool ProcessMouseWheelEvent(vtkMRMLInteractionEventData* eventData);

private:
  vtkMRMLSequenceLightboxDisplayableManager(const vtkMRMLSequenceLightboxDisplayableManager&) = delete;
  void operator=(const vtkMRMLSequenceLightboxDisplayableManager&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
