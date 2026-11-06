/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __vtkMRMLSequenceLightboxViewNode_h
#define __vtkMRMLSequenceLightboxViewNode_h

#include "vtkSlicerSequenceLightboxModuleMRMLExport.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>

// STD includes
#include <string>
#include <vector>

class vtkMRMLSequenceBrowserNode;

/// \brief MRML node for a "lightbox" view that tiles items of a sequence.
///
/// The lightbox view displays a grid of tiles. Each tile shows the same content as a
/// reference view (a slice view or a 3D view), except that the proxy nodes of the
/// referenced sequence browser node contain the data of a different sequence item.
///
/// The lightbox view does not add any node to the scene: each tile renders a private
/// scene that mirrors the relevant part of the main scene (with identical node IDs)
/// using the standard displayable managers.
///
/// The view follows the reference view: changing the slice position, zoom, window/level,
/// or the 3D camera in the reference view updates all tiles.
class VTK_SLICER_SEQUENCELIGHTBOX_MODULE_MRML_EXPORT vtkMRMLSequenceLightboxViewNode : public vtkMRMLAbstractViewNode
{
public:
  static vtkMRMLSequenceLightboxViewNode* New();
  vtkTypeMacro(vtkMRMLSequenceLightboxViewNode, vtkMRMLAbstractViewNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Events fired by this node when the referenced nodes are changed or modified.
  enum
  {
    /// Invoked when the referenced sequence browser node is changed or modified
    /// (for example a new item is selected or the sequences are modified).
    SequenceBrowserNodeModifiedEvent = 21100,
    /// Invoked when the referenced view node is changed or modified
    /// (for example the slice is scrolled or the camera is moved).
    ReferenceViewNodeModifiedEvent
  };

  /// Determines which sequence items are displayed in the tiles.
  enum ItemSelectionModeType
  {
    /// Consecutive items (separated by ItemStep) starting at StartItemNumber.
    ItemSelectionFixed = 0,
    /// Consecutive items (separated by ItemStep) starting at the item that is currently
    /// selected in the browser node, so the grid follows playback.
    ItemSelectionFollowSelected,
    /// Only the items listed in CustomItemNumbers, in the given order. Use it to display an
    /// arbitrary set of time points.
    ItemSelectionCustom,
    /// Consecutive items (separated by ItemStep) around the item that is currently selected in
    /// the browser node: the selected item is shown in the center tile and the tiles before and
    /// after it show the previous and the next items. The selected item stays in the same tile
    /// during playback, which makes this mode suitable for a timeline strip.
    ItemSelectionCenterSelected,
    NumberOfItemSelectionModes // this line must be the last one
  };

  //--------------------------------------------------------------------------
  /// MRMLNode methods
  //--------------------------------------------------------------------------

  vtkMRMLNode* CreateNodeInstance() override;

  /// Read node attributes from XML file.
  void ReadXMLAttributes(const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLSequenceLightboxViewNode);

  /// Get node XML tag name (like Volume, Model).
  const char* GetNodeTagName() override { return "SequenceLightboxView"; }

  /// Method to propagate events generated in mrml.
  void ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData) override;

  //--------------------------------------------------------------------------
  /// Referenced nodes
  //--------------------------------------------------------------------------

  /// The sequence browser node that defines which sequences are tiled and how the
  /// sequence item index is formatted for display.
  void SetAndObserveSequenceBrowserNodeID(const char* sequenceBrowserNodeID);
  const char* GetSequenceBrowserNodeID();
  vtkMRMLSequenceBrowserNode* GetSequenceBrowserNode();

  /// The view that the tiles follow. It is either a vtkMRMLSliceNode (each tile shows
  /// the same slice of a different sequence item) or a vtkMRMLViewNode (each tile shows
  /// the same 3D view of a different sequence item).
  void SetAndObserveReferenceViewNodeID(const char* referenceViewNodeID);
  const char* GetReferenceViewNodeID();
  vtkMRMLAbstractViewNode* GetReferenceViewNode();

  /// Returns true if the reference view node is a 3D view node.
  /// Returns false if it is a slice view node or if there is no reference view node.
  bool IsThreeDMode();

  virtual const char* GetSequenceBrowserNodeReferenceRole();
  virtual const char* GetReferenceViewNodeReferenceRole();

  //--------------------------------------------------------------------------
  /// Grid layout and content
  //--------------------------------------------------------------------------

  /// Number of tile rows.
  vtkGetMacro(Rows, int);
  vtkSetClampMacro(Rows, int, 1, MaximumGridSize);

  /// Number of tile columns.
  vtkGetMacro(Columns, int);
  vtkSetClampMacro(Columns, int, 1, MaximumGridSize);
  /// Width of the gap between the tiles, in pixels. The background of the view is visible in the gap.
  vtkGetMacro(TileSpacing, int);
  vtkSetClampMacro(TileSpacing, int, 0, 50);

  /// Difference between the sequence item numbers shown in consecutive tiles.
  vtkGetMacro(ItemStep, int);
  vtkSetClampMacro(ItemStep, int, 1, VTK_INT_MAX);

  /// Determines which sequence items are displayed in the tiles.
  /// \sa ItemSelectionModeType
  vtkGetMacro(ItemSelectionMode, int);
  vtkSetClampMacro(ItemSelectionMode, int, 0, vtkMRMLSequenceLightboxViewNode::NumberOfItemSelectionModes - 1);

  /// Sequence item shown in the first tile if ItemSelectionMode is ItemSelectionFixed.
  vtkGetMacro(StartItemNumber, int);
  vtkSetClampMacro(StartItemNumber, int, 0, VTK_INT_MAX);

  /// Sequence items shown in the tiles if ItemSelectionMode is ItemSelectionCustom.
  /// The first item of the list is displayed in the first tile, and so on. Items that do not
  /// exist in the sequence, and tiles that have no item in the list, are displayed empty.
  /// \sa SetCustomItemNumbersFromString
  const std::vector<int>& GetCustomItemNumbers() const { return this->CustomItemNumbers; }
  void SetCustomItemNumbers(const std::vector<int>& itemNumbers);

  /// Set the displayed items from a human readable list of item numbers, such as
  /// "0, 3, 7-9" (which is the same as "0, 3, 7, 8, 9"). Invalid values are ignored, and at most
  /// MaximumNumberOfCustomItems items are kept (a grid cannot display more tiles than that).
  /// \sa GetCustomItemNumbers
  void SetCustomItemNumbersFromString(const std::string& itemNumbers);
  /// Maximum number of items that SetCustomItemNumbersFromString keeps.
  static constexpr std::size_t MaximumNumberOfCustomItems = 1000;
  /// Get the displayed items as a human readable list of item numbers, using ranges where
  /// possible, such as "0, 3, 7-9".
  std::string GetCustomItemNumbersAsString();

  /// If enabled then the index value of the displayed item is shown in each tile.
  vtkGetMacro(ShowItemLabels, bool);
  vtkSetMacro(ShowItemLabels, bool);
  vtkBooleanMacro(ShowItemLabels, bool);

  /// If enabled then the tile that shows the currently selected sequence item is
  /// highlighted by a border.
  vtkGetMacro(HighlightSelectedItem, bool);
  vtkSetMacro(HighlightSelectedItem, bool);
  vtkBooleanMacro(HighlightSelectedItem, bool);

  /// If enabled then the tiles can be interacted with like regular views: the slice can be
  /// scrolled, panned and zoomed, the 3D camera can be rotated, markups can be placed and
  /// moved, etc. Changes made in a tile are applied to the scene: changes of the view are
  /// applied to the reference view (so all tiles follow), and changes of a sequence item
  /// are stored in that item of the sequence.
  /// If disabled then clicking a tile only selects its item in the browser node.
  vtkGetMacro(InteractiveTiles, bool);
  vtkSetMacro(InteractiveTiles, bool);
  vtkBooleanMacro(InteractiveTiles, bool);

  /// Returns the number of tiles (Rows * Columns).
  int GetNumberOfTiles();
  /// Set the number of rows and columns so that the specified number of items fit in the grid,
  /// with tiles as large as possible for a view of the specified size (in pixels, the aspect
  /// ratio is all that matters) and tiles that are approximately square. The number of rows and
  /// columns is limited to the maximum, so a very long sequence does not fit entirely.
  /// Returns true if all items fit.
  bool FitGridToItems(int numberOfItems, double viewWidth, double viewHeight);
  /// Maximum number of rows and columns.
  static constexpr int MaximumGridSize = 16;

  /// Returns the sequence item number displayed in the specified tile,
  /// or -1 if the tile does not show a valid item.
  /// Tiles are numbered row by row, starting from the top-left tile.
  int GetItemNumberForTile(int tileIndex);

  /// Returns the tile index that shows the specified sequence item number,
  /// or -1 if the item is not displayed in any tile.
  int GetTileForItemNumber(int itemNumber);

  /// Returns the sequence item number shown in the first tile for the current item selection
  /// mode, or -1 if there is no browser node or the mode is ItemSelectionCustom.
  int GetFirstItemNumber();

  /// Convert between item selection mode ID and name.
  static const char* GetItemSelectionModeAsString(int id);
  static int GetItemSelectionModeFromString(const char* name);

protected:
  vtkMRMLSequenceLightboxViewNode();
  ~vtkMRMLSequenceLightboxViewNode() override;
  vtkMRMLSequenceLightboxViewNode(const vtkMRMLSequenceLightboxViewNode&);
  void operator=(const vtkMRMLSequenceLightboxViewNode&);

  virtual const char* GetSequenceBrowserNodeReferenceMRMLAttributeName();
  virtual const char* GetReferenceViewNodeReferenceMRMLAttributeName();

  /// Called when a node reference ID is added (list size increased).
  void OnNodeReferenceAdded(vtkMRMLNodeReference* reference) override;

  /// Called when a node reference ID is modified.
  void OnNodeReferenceModified(vtkMRMLNodeReference* reference) override;

  /// Called after a node reference ID is removed (list size decreased).
  void OnNodeReferenceRemoved(vtkMRMLNodeReference* reference) override;

  /// Invokes SequenceBrowserNodeModifiedEvent or ReferenceViewNodeModifiedEvent
  /// if the reference role matches.
  void InvokeEventForReferenceRole(const char* referenceRole, vtkMRMLNode* referencedNode);

  static const char* SequenceBrowserNodeReferenceRole;
  static const char* SequenceBrowserNodeReferenceMRMLAttributeName;
  static const char* ReferenceViewNodeReferenceRole;
  static const char* ReferenceViewNodeReferenceMRMLAttributeName;

  int Rows{ 2 };
  int Columns{ 3 };
  int TileSpacing{ 2 };
  int ItemStep{ 1 };
  int ItemSelectionMode{ ItemSelectionFollowSelected };
  int StartItemNumber{ 0 };
  std::vector<int> CustomItemNumbers;
  bool ShowItemLabels{ true };
  bool HighlightSelectedItem{ true };
  bool InteractiveTiles{ true };
};

#endif
