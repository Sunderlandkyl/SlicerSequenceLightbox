/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __vtkSequenceLightboxSceneMirror_h
#define __vtkSequenceLightboxSceneMirror_h

#include "vtkSlicerSequenceLightboxModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// STD includes
#include <map>
#include <set>
#include <string>
#include <vector>

class vtkCallbackCommand;
class vtkMRMLAbstractViewNode;
class vtkMRMLNode;
class vtkMRMLScene;
class vtkMRMLMarkupsNode;
class vtkMRMLSequenceBrowserNode;
class vtkMRMLSliceCompositeNode;

/// \brief Maintains a private scene that mirrors a part of a main MRML scene.
///
/// The mirror scene contains a shallow copy of every node that is needed to render the
/// content of a reference view (a slice view or a 3D view), with two differences:
/// - the proxy nodes of a sequence browser node contain the data of the sequence item
///   specified by SetItemNumber() instead of the currently selected item,
/// - nodes that are irrelevant for rendering (sequences, storage nodes, subject hierarchy,
///   other views, ...) are not mirrored.
///
/// Mirrored nodes keep the node ID that they have in the source scene, therefore all node
/// references between them are resolved in the mirror scene without any modification.
/// Bulk data (image data, meshes, segmentations, transforms) is shared with the source
/// scene, so mirroring a scene is inexpensive.
///
/// This makes it possible to render a sequence item using the standard displayable managers,
/// without adding any node to the scene that the user works with.
///
/// The mirror does not update itself automatically: when the source scene or any mirrored
/// node changes then vtkCommand::ModifiedEvent is invoked and Update() must be called.
///
/// The mirror scene can also be interacted with, for example by the displayable managers that
/// render it: while write-back is enabled (see SetWriteBackEnabled), modifications of the
/// mirrored nodes are written back to the source scene. Modifications of a proxy node mirror
/// are stored in the sequence item that the mirror shows (and in the proxy node itself if that
/// item is the selected one), modifications of any other node are applied to the source node.
class VTK_SLICER_SEQUENCELIGHTBOX_MODULE_LOGIC_EXPORT vtkSequenceLightboxSceneMirror : public vtkObject
{
public:
  static vtkSequenceLightboxSceneMirror* New();
  vtkTypeMacro(vtkSequenceLightboxSceneMirror, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// The scene that is mirrored.
  void SetSourceScene(vtkMRMLScene* scene);
  vtkMRMLScene* GetSourceScene();

  /// The sequence browser node whose proxy nodes are replaced by sequence item data.
  void SetSequenceBrowserNode(vtkMRMLSequenceBrowserNode* browserNode);
  vtkMRMLSequenceBrowserNode* GetSequenceBrowserNode();

  /// The view that determines what is mirrored. It is either a vtkMRMLSliceNode or a
  /// vtkMRMLViewNode.
  void SetReferenceViewNode(vtkMRMLAbstractViewNode* viewNode);
  vtkMRMLAbstractViewNode* GetReferenceViewNode();

  /// Sequence item number that the proxy nodes are set to.
  /// If the item number is invalid (for example -1) then the proxy nodes are hidden.
  void SetItemNumber(int itemNumber);
  vtkGetMacro(ItemNumber, int);

  /// Number of times the source scene has been scanned to determine the set of nodes that have
  /// to be mirrored. Scanning the source scene is the most expensive part of an update, so it is
  /// only done when that set may have changed. Exposed for testing and troubleshooting.
  vtkGetMacro(SourceSceneScanCount, unsigned long);

  /// The private scene that contains the mirrored nodes. Created on demand.
  vtkMRMLScene* GetTileScene();

  /// Mirror of the reference view node, in the mirror scene.
  vtkMRMLAbstractViewNode* GetTileViewNode();

  /// Mirror of the slice composite node of the reference slice view, in the mirror scene.
  /// Returns nullptr if the reference view is not a slice view.
  vtkMRMLSliceCompositeNode* GetTileSliceCompositeNode();

  /// Mirror of the specified source scene node, in the mirror scene.
  vtkMRMLNode* GetMirrorNode(const char* sourceNodeID);

  /// Synchronize the mirror scene with the source scene.
  /// Nodes are added, removed and updated as needed. Content is only copied for nodes that
  /// have been modified since the last update, therefore calling this method repeatedly is
  /// inexpensive.
  void Update();

  /// Request a full update: content of all mirrored nodes is copied on the next Update(),
  /// even if the source nodes have not been modified.
  void Modified() override;

  /// Enable or disable writing modifications of the mirrored nodes back to the source scene.
  /// Write-back is disabled by default: the mirror scene is then read-only, and any change made
  /// to it is overwritten by the next update. It is intended to be enabled only while the user
  /// interacts with the mirror scene (for example while a displayable manager processes an
  /// interaction event), so that modifications made by the rendering pipeline are not written
  /// back. Disabling write-back writes back the pending modifications.
  /// \sa WriteBackModifiedNodes
  void SetWriteBackEnabled(bool enabled);
  vtkGetMacro(WriteBackEnabled, bool);

  /// Write the content of the mirrored nodes that have been modified in the mirror scene since
  /// the last write-back to the source scene. Modifications are collected while write-back is
  /// enabled. Invokes vtkCommand::ModifiedEvent if anything was written, because other mirrors
  /// of the same source scene may have to be updated.
  void WriteBackModifiedNodes();

  /// Number of mirrored nodes that have been written back to the source scene so far.
  /// Exposed for testing and troubleshooting.
  vtkGetMacro(WriteBackCount, unsigned long);

  /// Returns the data node of the sequence item that the mirror of the specified proxy node
  /// shows, or nullptr if the node is not a proxy node of the browser node, the item is invalid,
  /// or the sequence does not contain a compatible node for the item.
  /// If the sequence has no item for the index value of the mirrored item then the previous
  /// item is returned if the missing item mode of the browser node is "create from previous"
  /// (the proxy node shows the previous item in that mode), nullptr otherwise.
  vtkMRMLNode* GetSequenceItemDataNode(vtkMRMLNode* sourceNode);

  /// Returns the index value (defined by the master sequence of the browser node) of the
  /// mirrored item. Returns false if the item number is invalid.
  bool GetSequenceItemIndexValue(std::string& indexValue);

  /// Returns true if the defined control points of the two markups nodes differ (number or
  /// position). Preview and undefined control points are ignored. A null reference node has
  /// no control points.
  static bool DefinedControlPointsDiffer(vtkMRMLMarkupsNode* markupsNode, vtkMRMLMarkupsNode* referenceMarkupsNode);

  /// Remove all mirrored nodes and stop observing the source scene.
  void Clear();

protected:
  vtkSequenceLightboxSceneMirror();
  ~vtkSequenceLightboxSceneMirror() override;

  /// Returns true if the node is needed for rendering the reference view content.
  bool IsNodeMirrored(vtkMRMLNode* sourceNode);

  /// Collect the IDs of all nodes that must be mirrored. Displayable nodes that are not shown in
  /// the reference view (and their display nodes) are collected in candidateNodes: they are
  /// watched, because they may become displayable in the reference view later.
  void CollectNodesToMirror(std::set<std::string>& sourceNodeIDs, std::vector<vtkMRMLNode*>& candidateNodes);

  /// Observe the nodes that are not mirrored but may have to be mirrored after they are modified
  /// (for example a model node that gets its display node after it was added to the scene).
  void UpdateWatchedNodes(const std::vector<vtkMRMLNode*>& candidateNodes);
  void RemoveAllWatchedNodeObservers();

  /// Add the node and all nodes that it references (recursively) to sourceNodeIDs.
  void CollectReferencedNodes(vtkMRMLNode* sourceNode, std::set<std::string>& sourceNodeIDs);

  /// Create the mirror scene if it does not exist yet.
  void CreateTileScene();

  /// Create a mirror of the specified source node in the mirror scene, keeping its node ID.
  vtkMRMLNode* CreateMirrorNode(vtkMRMLNode* sourceNode);

  /// Copy content and references of the source node into its mirror, if needed.
  void UpdateMirrorNode(vtkMRMLNode* sourceNode, vtkMRMLNode* mirrorNode);

  /// Returns the node that the content of the mirror of the specified node is copied from:
  /// the sequence item data node for proxy nodes (or the proxy node itself if it shows the
  /// selected item, so that unsaved changes of the proxy node are displayed), the source node
  /// itself otherwise.
  vtkMRMLNode* GetContentSourceNode(vtkMRMLNode* sourceNode);

  /// Returns true if the mirrored item is the item that is selected in the browser node.
  bool IsSelectedItem();

  /// Returns true if the two nodes do not have the same node references.
  static bool NodeReferencesDiffer(vtkMRMLNode* node1, vtkMRMLNode* node2);

  /// Copy the content of a node into another node of the same class, choosing the copy method
  /// that is appropriate for the node class.
  static void CopyNodeContent(vtkMRMLNode* targetNode, vtkMRMLNode* sourceNode);
  /// Returns the XML attributes of the node, which describe the content of a display node.
  static std::string GetContentXML(vtkMRMLNode* node);

  /// Returns a string that describes everything about the node that determines which nodes have
  /// to be mirrored: its node references and, for display nodes, the views it is displayed in.
  /// If a modified node has the same signature as before then the set of nodes to mirror cannot
  /// have changed, so the expensive scan of the source scene is skipped.
  static std::string ComputeStructureSignature(vtkMRMLNode* node);

  /// Apply the visibility of the mirrored proxy nodes: they are visible if the source node is
  /// visible and the sequence contains data for the current item.
  void UpdateProxyNodeVisibility();

  void AddSourceNodeObserver(vtkMRMLNode* sourceNode);
  void RemoveSourceNodeObserver(vtkMRMLNode* sourceNode);
  void RemoveAllSourceNodeObservers();

  static void SourceModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  /// Invoked when a mirrored node is modified in the mirror scene.
  static void MirrorModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  /// Invoke an event that was invoked on a mirror node (such as a request to show the context
  /// menu of a display node) on the source node, so that the application handles it as if it
  /// came from a regular view. If the node belongs to a sequence item that is not the selected
  /// one then the item is selected first, so that the proxy node that the application acts on
  /// contains that item.
  void ForwardMirrorNodeEvent(vtkMRMLNode* mirrorNode, unsigned long eid, void* callData);

  /// Copy the content of the mirror node to the node(s) that it was mirrored from.
  struct MirroredNode;
  void WriteBackMirrorNode(MirroredNode& mirroredNode);

  vtkWeakPointer<vtkMRMLScene> SourceScene;
  vtkSmartPointer<vtkMRMLScene> TileScene;
  vtkWeakPointer<vtkMRMLSequenceBrowserNode> SequenceBrowserNode;
  vtkWeakPointer<vtkMRMLAbstractViewNode> ReferenceViewNode;

  /// Cached nodes that belong to the reference view. Updated at the beginning of each update,
  /// used to decide which camera and slice composite node is mirrored.
  vtkWeakPointer<vtkMRMLNode> ReferenceCameraNode;
  vtkWeakPointer<vtkMRMLNode> ReferenceSliceCompositeNode;

  /// Update ReferenceCameraNode and ReferenceSliceCompositeNode from the reference view node.
  void UpdateReferenceViewRelatedNodes();

  int ItemNumber{ -1 };

  /// Selected item of the browser node at the last update. The content source of a proxy node
  /// mirror depends on whether the mirrored item is the selected one.
  int LastSelectedItemNumber{ -1 };

  /// True if modifications of the mirrored nodes are written back to the source scene.
  bool WriteBackEnabled{ false };

  /// Source node IDs of the mirrored nodes that have been modified in the mirror scene and have
  /// not been written back yet.
  std::vector<std::string> PendingWriteBackNodeIDs;

  /// Number of write-backs performed so far. See GetWriteBackCount().
  unsigned long WriteBackCount{ 0 };

  /// True if the content of all mirrored nodes must be copied on the next update.
  bool FullUpdateNeeded{ true };

  /// True if the set of nodes that have to be mirrored may have changed, therefore the source
  /// scene has to be scanned again on the next update. Scanning the source scene is the most
  /// expensive part of an update, so it is only done when something may have changed.
  bool SourceStructureModified{ true };

  /// Number of times the source scene has been scanned. See GetSourceSceneScanCount().
  unsigned long SourceSceneScanCount{ 0 };

  /// True if the mirrored item changed, therefore the content of the proxy node mirrors has to
  /// be copied from a different sequence item on the next update.
  bool ItemNumberModified{ true };

  /// True if any mirrored source node invoked a modified event since the last update.
  bool MirroredNodeContentModified{ false };

  /// True if any mirrored node (or the sequence item that a proxy node mirror shows) has been
  /// modified since the last update. Only meaningful if the flags above are all false.
  bool IsAnyMirroredNodeModified();

  /// Modified time of the browser node at the last update. The item that a proxy node mirror
  /// shows depends on properties of the browser node (for example the missing item mode).
  vtkMTimeType LastBrowserNodeMTime{ 0 };

  /// State of a single mirrored node.
  struct MirroredNode
  {
    vtkSmartPointer<vtkMRMLNode> MirrorNode;
    vtkWeakPointer<vtkMRMLNode> SourceNode;
    std::vector<unsigned long> ObservationTags;
    /// Observations of the mirror node, used to detect modifications that have to be written back.
    std::vector<unsigned long> MirrorObservationTags;
    /// True if the mirror node has been modified and has not been written back yet.
    bool WriteBackPending{ false };
    /// True if the source node invoked a modified event since the last update. Some nodes (for
    /// example markups nodes) only invoke custom content modified events, which do not update
    /// their modified time, so the modified time alone cannot tell that the content changed.
    bool SourceModified{ false };
    /// Signature of the source node at the last update, see ComputeStructureSignature.
    std::string StructureSignature;
    /// Node that the content was last copied from (a sequence item data node for proxy nodes).
    /// Sequence item data nodes are deleted with their sequence, which the mirror is not notified
    /// about, so the node must not be dereferenced once it is gone.
    vtkWeakPointer<vtkMRMLNode> LastContentSourceNode;
    vtkMTimeType LastContentMTime{ 0 };
    vtkMTimeType LastReferenceMTime{ 0 };
    /// Display nodes: XML attributes of the content source at the last copy. The copy is skipped
    /// if the attributes did not change, see UpdateMirrorNode.
    std::string LastContentXML;
    /// True if the node is a proxy node of the observed browser node.
    bool IsProxyNode{ false };
    /// True if the sequence does not contain data for the current item.
    bool ItemMissing{ false };
    /// True if the content of the mirror node has been reset to the default (empty) content
    /// because the item is missing.
    bool ContentIsDefault{ false };
    /// Proxy nodes: modified time of the sequence node at the last update. Items may be added
    /// to or removed from the sequence (for example when recording), which changes the item
    /// that the mirror shows without modifying any mirrored node.
    vtkMTimeType LastSequenceMTime{ 0 };
    /// Proxy nodes: modified time of the proxy node right after the browser node updated it from
    /// the sequence (when the selection changed), 0 if unknown. If the proxy node is newer then
    /// it may contain changes that are not saved into the sequence, which the tile that shows the
    /// selected item must display. See GetContentSourceNode.
    vtkMTimeType CleanProxyNodeMTime{ 0 };
  };

  /// Mirrored nodes, indexed by source node ID.
  std::map<std::string, MirroredNode> MirroredNodes;

  /// Observation of a source node that is not mirrored, but may have to be mirrored later.
  struct WatchedNode
  {
    vtkWeakPointer<vtkMRMLNode> Node;
    unsigned long ObservationTag{ 0 };
  };
  /// Watched nodes, indexed by source node ID.
  std::map<std::string, WatchedNode> WatchedNodes;

  vtkSmartPointer<vtkCallbackCommand> SourceCallbackCommand;
  vtkSmartPointer<vtkCallbackCommand> MirrorCallbackCommand;
  std::vector<unsigned long> SourceSceneObservationTags;

  /// Prevents recursive updates.
  bool UpdateInProgress{ false };

private:
  vtkSequenceLightboxSceneMirror(const vtkSequenceLightboxSceneMirror&) = delete;
  void operator=(const vtkSequenceLightboxSceneMirror&) = delete;
};

#endif
