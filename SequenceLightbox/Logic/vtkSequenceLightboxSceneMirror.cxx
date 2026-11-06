/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Sequences Logic includes
#include "vtkSequenceLightboxSceneMirror.h"

// Sequences MRML includes
#include "vtkMRMLSequenceBrowserNode.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLMarkupsNode.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSequenceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>

// MRMLLogic includes
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLViewLogic.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cstring>
#include <set>
#include <sstream>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSequenceLightboxSceneMirror);

//----------------------------------------------------------------------------
vtkSequenceLightboxSceneMirror::vtkSequenceLightboxSceneMirror()
{
  this->SourceCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->SourceCallbackCommand->SetClientData(this);
  this->SourceCallbackCommand->SetCallback(vtkSequenceLightboxSceneMirror::SourceModifiedCallback);

  this->MirrorCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->MirrorCallbackCommand->SetClientData(this);
  this->MirrorCallbackCommand->SetCallback(vtkSequenceLightboxSceneMirror::MirrorModifiedCallback);
}

//----------------------------------------------------------------------------
vtkSequenceLightboxSceneMirror::~vtkSequenceLightboxSceneMirror()
{
  this->Clear();
  this->SetSourceScene(nullptr);
  this->TileScene = nullptr;
  if (this->SourceCallbackCommand)
  {
    this->SourceCallbackCommand->SetClientData(nullptr);
  }
  if (this->MirrorCallbackCommand)
  {
    this->MirrorCallbackCommand->SetClientData(nullptr);
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ItemNumber: " << this->ItemNumber << "\n";
  os << indent << "Number of mirrored nodes: " << this->MirroredNodes.size() << "\n";
  os << indent << "WriteBackEnabled: " << (this->WriteBackEnabled ? "true" : "false") << "\n";
  os << indent << "WriteBackCount: " << this->WriteBackCount << "\n";
  os << indent << "Reference view node: " << (this->ReferenceViewNode ? this->ReferenceViewNode->GetID() : "(none)") << "\n";
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::Modified()
{
  this->FullUpdateNeeded = true;
  this->SourceStructureModified = true;
  this->Superclass::Modified();
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SourceModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* vtkNotUsed(callData))
{
  vtkSequenceLightboxSceneMirror* self = reinterpret_cast<vtkSequenceLightboxSceneMirror*>(clientData);
  if (!self || self->UpdateInProgress)
  {
    return;
  }
  vtkMRMLNode* sourceNode = vtkMRMLNode::SafeDownCast(caller);
  if (sourceNode)
  {
    // A mirrored node was modified: its content is copied on the next update, whether or not its
    // modified time changed (custom content modified events do not update the modified time).
    std::map<std::string, MirroredNode>::iterator it = (sourceNode->GetID() ? self->MirroredNodes.find(sourceNode->GetID()) : self->MirroredNodes.end());
    bool mirrored = (it != self->MirroredNodes.end() && it->second.SourceNode == sourceNode);
    if (mirrored)
    {
      it->second.SourceModified = true;
      self->MirroredNodeContentModified = true;
    }
    if (eid == vtkCommand::ModifiedEvent)
    {
      // The node references of the node may have changed (for example a display node was added),
      // in which case the set of nodes to mirror is recomputed on the next update. Custom content
      // modified events (for example a markup control point was moved) do not change node
      // references. Most modified events of mirrored nodes do not change them either (for example
      // scrolling a slice view modifies the slice node): the scan of the source scene is the most
      // expensive part of an update, so it is only requested if the node references (or the views
      // of a display node) actually changed.
      if (!mirrored || vtkSequenceLightboxSceneMirror::ComputeStructureSignature(sourceNode) != it->second.StructureSignature)
      {
        self->SourceStructureModified = true;
      }
    }
  }
  else
  {
    // A node was added to or removed from the source scene (or the scene was imported).
    self->SourceStructureModified = true;
  }
  // Only notify observers. The mirror is synchronized when Update() is called.
  self->InvokeEvent(vtkCommand::ModifiedEvent);
}

//----------------------------------------------------------------------------
std::string vtkSequenceLightboxSceneMirror::ComputeStructureSignature(vtkMRMLNode* node)
{
  if (!node)
  {
    return "";
  }
  std::string signature;
  std::vector<std::string> roles;
  node->GetNodeReferenceRoles(roles);
  for (const std::string& role : roles)
  {
    std::vector<const char*> referencedNodeIDs;
    node->GetNodeReferenceIDs(role.c_str(), referencedNodeIDs);
    if (referencedNodeIDs.empty())
    {
      continue;
    }
    signature += role;
    signature += '=';
    for (const char* referencedNodeID : referencedNodeIDs)
    {
      signature += (referencedNodeID ? referencedNodeID : "");
      signature += ',';
    }
    signature += ';';
  }
  vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(node);
  if (displayNode)
  {
    // The color node is referenced by ID, and the view node IDs determine whether the displayable
    // node of the display node is mirrored at all.
    signature += "color=";
    signature += (displayNode->GetColorNodeID() ? displayNode->GetColorNodeID() : "");
    signature += ";views=";
    int numberOfViewNodeIDs = displayNode->GetNumberOfViewNodeIDs();
    for (int viewNodeIndex = 0; viewNodeIndex < numberOfViewNodeIDs; ++viewNodeIndex)
    {
      const char* viewNodeID = displayNode->GetNthViewNodeID(viewNodeIndex);
      signature += (viewNodeID ? viewNodeID : "");
      signature += ',';
    }
  }
  return signature;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::CopyNodeContent(vtkMRMLNode* targetNode, vtkMRMLNode* sourceNode)
{
  if (!targetNode || !sourceNode)
  {
    return;
  }
  if (vtkMRMLColorNode::SafeDownCast(targetNode)          //
      || vtkMRMLInteractionNode::SafeDownCast(targetNode) //
      || vtkMRMLSelectionNode::SafeDownCast(targetNode))
  {
    // Color nodes generate their lookup table from their type, and CopyContent only copies the
    // type (creating the table is expensive, so it is deliberately not done there). Copy() of the
    // color node classes does copy the table, and color nodes are small.
    // Interaction and selection nodes do not implement CopyContent at all, only Copy().
    targetNode->Copy(sourceNode);
    return;
  }
  // Shallow copy: bulk data (image data, mesh, segmentation, transform) is shared between the
  // source scene and the mirror scene, so copying a node is inexpensive.
  targetNode->CopyContent(sourceNode, /*deepCopy=*/false);
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::MirrorModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData)
{
  vtkSequenceLightboxSceneMirror* self = reinterpret_cast<vtkSequenceLightboxSceneMirror*>(clientData);
  if (self && (eid == vtkMRMLDisplayNode::MenuEvent || eid == vtkMRMLInteractionNode::ShowViewContextMenuEvent))
  {
    // Requests to show a context menu are handled by the application, which only observes the
    // nodes of its own scene.
    self->ForwardMirrorNodeEvent(vtkMRMLNode::SafeDownCast(caller), eid, callData);
    return;
  }
  if (!self || self->UpdateInProgress || !self->WriteBackEnabled)
  {
    // Modifications made by this class (copying content from the source scene) and
    // modifications made while write-back is disabled (for example by the rendering pipeline
    // of the mirror scene) are not written back.
    return;
  }
  vtkMRMLNode* mirrorNode = vtkMRMLNode::SafeDownCast(caller);
  if (!mirrorNode || !mirrorNode->GetID())
  {
    return;
  }
  std::map<std::string, MirroredNode>::iterator it = self->MirroredNodes.find(mirrorNode->GetID());
  if (it == self->MirroredNodes.end() || it->second.MirrorNode != mirrorNode)
  {
    return;
  }
  if (!it->second.WriteBackPending)
  {
    it->second.WriteBackPending = true;
    self->PendingWriteBackNodeIDs.push_back(it->first);
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::ForwardMirrorNodeEvent(vtkMRMLNode* mirrorNode, unsigned long eid, void* callData)
{
  if (!mirrorNode || !mirrorNode->GetID())
  {
    return;
  }
  std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(mirrorNode->GetID());
  if (it == this->MirroredNodes.end() || it->second.MirrorNode != mirrorNode || !it->second.SourceNode)
  {
    return;
  }
  vtkMRMLNode* sourceNode = it->second.SourceNode;

  // The menu of a display node acts on the displayable node of the application (the proxy node
  // for sequence items). The proxy node must contain the item that the tile shows, otherwise the
  // actions of the menu (for example deleting a control point) would be applied to another item.
  vtkMRMLDisplayNode* sourceDisplayNode = vtkMRMLDisplayNode::SafeDownCast(sourceNode);
  vtkMRMLSequenceBrowserNode* browserNode = this->SequenceBrowserNode;
  if (sourceDisplayNode && browserNode && this->ItemNumber >= 0 && !this->IsSelectedItem())
  {
    vtkMRMLDisplayableNode* sourceDisplayableNode = sourceDisplayNode->GetDisplayableNode();
    if (sourceDisplayableNode && browserNode->GetSequenceNode(sourceDisplayableNode))
    {
      browserNode->SetSelectedItemNumber(this->ItemNumber);
    }
  }
  sourceNode->InvokeEvent(eid, callData);
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SetWriteBackEnabled(bool enabled)
{
  if (this->WriteBackEnabled == enabled)
  {
    return;
  }
  if (!enabled)
  {
    // Modifications that were collected while write-back was enabled are applied now, so that
    // the source scene is up to date when the interaction is over.
    this->WriteBackModifiedNodes();
  }
  this->WriteBackEnabled = enabled;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::WriteBackModifiedNodes()
{
  if (this->PendingWriteBackNodeIDs.empty())
  {
    return;
  }
  // Writing back may modify further nodes (which must not be written back again), therefore the
  // pending list is detached first.
  std::vector<std::string> pendingNodeIDs;
  pendingNodeIDs.swap(this->PendingWriteBackNodeIDs);
  bool anyWritten = false;
  for (const std::string& sourceNodeID : pendingNodeIDs)
  {
    std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(sourceNodeID);
    if (it == this->MirroredNodes.end())
    {
      continue;
    }
    it->second.WriteBackPending = false;
    this->WriteBackMirrorNode(it->second);
    anyWritten = true;
  }
  if (anyWritten)
  {
    // Other mirrors of the same source scene (the other tiles) may show the same nodes.
    this->InvokeEvent(vtkCommand::ModifiedEvent);
  }
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::NodeReferencesDiffer(vtkMRMLNode* node1, vtkMRMLNode* node2)
{
  if (!node1 || !node2)
  {
    return node1 != node2;
  }
  std::vector<std::string> roles1;
  node1->GetNodeReferenceRoles(roles1);
  std::vector<std::string> roles2;
  node2->GetNodeReferenceRoles(roles2);
  // Roles without references are irrelevant, so the role lists themselves are not compared.
  std::set<std::string> roles(roles1.begin(), roles1.end());
  roles.insert(roles2.begin(), roles2.end());
  for (const std::string& role : roles)
  {
    std::vector<const char*> referencedNodeIDs1;
    node1->GetNodeReferenceIDs(role.c_str(), referencedNodeIDs1);
    std::vector<const char*> referencedNodeIDs2;
    node2->GetNodeReferenceIDs(role.c_str(), referencedNodeIDs2);
    if (referencedNodeIDs1.size() != referencedNodeIDs2.size())
    {
      return true;
    }
    for (std::size_t referenceIndex = 0; referenceIndex < referencedNodeIDs1.size(); ++referenceIndex)
    {
      const char* id1 = referencedNodeIDs1[referenceIndex];
      const char* id2 = referencedNodeIDs2[referenceIndex];
      if ((id1 == nullptr) != (id2 == nullptr))
      {
        return true;
      }
      if (id1 && id2 && std::strcmp(id1, id2) != 0)
      {
        return true;
      }
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::WriteBackMirrorNode(MirroredNode& mirroredNode)
{
  vtkMRMLNode* sourceNode = mirroredNode.SourceNode;
  vtkMRMLNode* mirrorNode = mirroredNode.MirrorNode;
  if (!sourceNode || !mirrorNode)
  {
    return;
  }

  // Nodes that receive the content of the mirror node.
  std::vector<vtkMRMLNode*> targetNodes;
  vtkMRMLSequenceNode* sequenceNode = nullptr;
  std::string indexValue;
  bool createItem = false;
  if (mirroredNode.IsProxyNode)
  {
    // Modifications of a sequence item are stored in the sequence, whether or not the browser
    // node saves the changes of its proxy nodes: the user explicitly edited that item.
    sequenceNode = (this->SequenceBrowserNode ? this->SequenceBrowserNode->GetSequenceNode(sourceNode) : nullptr);
    if (!sequenceNode || !this->GetSequenceItemIndexValue(indexValue))
    {
      return;
    }
    vtkMRMLNode* dataNode = sequenceNode->GetDataNodeAtValue(indexValue, /*exactMatchRequired=*/true);
    if (dataNode)
    {
      if (std::strcmp(dataNode->GetClassName(), mirrorNode->GetClassName()) != 0)
      {
        // The sequence contains a different node type, it cannot be modified.
        return;
      }
      targetNodes.push_back(dataNode);
    }
    else
    {
      // The sequence has no item for the index value of this tile (the tile showed the previous
      // item, or nothing). The item is created from the edited content, as when the changes of
      // a proxy node are saved into a sequence that does not have an item for the current index.
      vtkMRMLNode* firstDataNode = (sequenceNode->GetNumberOfDataNodes() > 0 ? sequenceNode->GetNthDataNode(0) : nullptr);
      if (firstDataNode && std::strcmp(firstDataNode->GetClassName(), mirrorNode->GetClassName()) != 0)
      {
        return;
      }
      // Moving the pointer over a tile in place mode already modifies the mirrored markups node
      // (the preview point follows the pointer). That must not add items to the sequence: an
      // item is only added once the defined control points differ from the ones that the tile
      // showed before (the previous item, or nothing): a point was placed, moved or removed.
      vtkMRMLMarkupsNode* mirrorMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(mirrorNode);
      if (mirrorMarkupsNode
          && !vtkSequenceLightboxSceneMirror::DefinedControlPointsDiffer(mirrorMarkupsNode, vtkMRMLMarkupsNode::SafeDownCast(mirroredNode.LastContentSourceNode.GetPointer())))
      {
        return;
      }
      createItem = true;
    }
    if (this->IsSelectedItem())
    {
      // The proxy node shows the same item, so it is updated as well (which updates the regular
      // views immediately).
      targetNodes.push_back(sourceNode);
    }
  }
  else
  {
    targetNodes.push_back(sourceNode);
  }

  // Modifications of the source scene that are caused by this write-back must not trigger
  // another update of this mirror (the mirror is the origin of these changes).
  bool wasUpdateInProgress = this->UpdateInProgress;
  this->UpdateInProgress = true;

  if (createItem)
  {
    // Deep-copies the content of the mirror node into the sequence.
    sequenceNode->SetDataNodeAtValue(mirrorNode, indexValue);
  }

  for (vtkMRMLNode* targetNode : targetNodes)
  {
    MRMLNodeModifyBlocker blocker(targetNode);
    // Slice views are linked through their composite node. Linking is disabled in the mirror
    // scene (a tile is a single view), and that must not be propagated to the source scene.
    vtkMRMLSliceCompositeNode* targetCompositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(targetNode);
    int linkedControl = (targetCompositeNode ? targetCompositeNode->GetLinkedControl() : 0);
    int hotLinkedControl = (targetCompositeNode ? targetCompositeNode->GetHotLinkedControl() : 0);
    // The visibility of a mirrored display node is derived from the source display node and
    // from the availability of the item (see UpdateProxyNodeVisibility): a tile hides the nodes
    // of a missing item. It is never written back, otherwise interacting with such a tile (which
    // modifies the display node, for example by highlighting the hovered component) would hide
    // the node in the application.
    vtkMRMLDisplayNode* targetDisplayNode = vtkMRMLDisplayNode::SafeDownCast(targetNode);
    int visibility = (targetDisplayNode ? targetDisplayNode->GetVisibility() : 0);
    vtkSequenceLightboxSceneMirror::CopyNodeContent(targetNode, mirrorNode);
    if (targetCompositeNode)
    {
      targetCompositeNode->SetLinkedControl(linkedControl);
      targetCompositeNode->SetHotLinkedControl(hotLinkedControl);
    }
    if (targetDisplayNode)
    {
      targetDisplayNode->SetVisibility(visibility);
    }
    // Some nodes (for example markups nodes) only invoke custom events when their content is
    // copied. A modified event is invoked explicitly so that every observer of the target node
    // (views, other mirrors, the sequence browser) is notified and the modified time is updated.
    targetNode->Modified();
  }
  if (sequenceNode)
  {
    // The sequence now contains unsaved changes.
    sequenceNode->StorableModified();
  }

  // References are only written back to the source node (sequence item data nodes do not
  // reference other nodes), and only if they actually changed: CopyReferences removes and
  // re-adds all references, which would notify every observer of the source node.
  if (vtkSequenceLightboxSceneMirror::NodeReferencesDiffer(sourceNode, mirrorNode))
  {
    MRMLNodeModifyBlocker blocker(sourceNode);
    sourceNode->CopyReferences(mirrorNode);
  }

  this->UpdateInProgress = wasUpdateInProgress;

  // The mirror node is now identical to its source: the next update must not copy it back.
  vtkMRMLNode* contentSourceNode = this->GetContentSourceNode(sourceNode);
  mirroredNode.LastContentSourceNode = contentSourceNode;
  mirroredNode.LastContentMTime = (contentSourceNode ? contentSourceNode->GetMTime() : 0);
  mirroredNode.LastReferenceMTime = sourceNode->GetMTime();
  mirroredNode.LastSequenceMTime = (sequenceNode ? sequenceNode->GetMTime() : 0);
  ++this->WriteBackCount;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SetSourceScene(vtkMRMLScene* scene)
{
  if (this->SourceScene == scene)
  {
    return;
  }
  if (this->SourceScene)
  {
    for (unsigned long tag : this->SourceSceneObservationTags)
    {
      this->SourceScene->RemoveObserver(tag);
    }
  }
  this->SourceSceneObservationTags.clear();
  this->Clear();
  this->SourceScene = scene;
  if (this->SourceScene)
  {
    int events[] = {
      vtkMRMLScene::NodeAddedEvent,      //
      vtkMRMLScene::NodeRemovedEvent,    //
      vtkMRMLScene::EndImportEvent,      //
      vtkMRMLScene::EndRestoreEvent,     //
      vtkMRMLScene::EndBatchProcessEvent //
    };
    for (int event : events)
    {
      this->SourceSceneObservationTags.push_back(this->SourceScene->AddObserver(event, this->SourceCallbackCommand));
    }
  }
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLScene* vtkSequenceLightboxSceneMirror::GetSourceScene()
{
  return this->SourceScene;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SetSequenceBrowserNode(vtkMRMLSequenceBrowserNode* browserNode)
{
  if (this->SequenceBrowserNode == browserNode)
  {
    return;
  }
  this->SequenceBrowserNode = browserNode;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLSequenceBrowserNode* vtkSequenceLightboxSceneMirror::GetSequenceBrowserNode()
{
  return this->SequenceBrowserNode;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SetReferenceViewNode(vtkMRMLAbstractViewNode* viewNode)
{
  if (this->ReferenceViewNode == viewNode)
  {
    return;
  }
  // The set of mirrored nodes and the identity of the mirrored view node change completely,
  // therefore the mirror scene is rebuilt from scratch.
  this->Clear();
  this->ReferenceViewNode = viewNode;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLAbstractViewNode* vtkSequenceLightboxSceneMirror::GetReferenceViewNode()
{
  return this->ReferenceViewNode;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::SetItemNumber(int itemNumber)
{
  if (this->ItemNumber == itemNumber)
  {
    return;
  }
  this->ItemNumber = itemNumber;
  this->ItemNumberModified = true;
  this->Superclass::Modified();
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::CreateTileScene()
{
  if (this->TileScene)
  {
    return;
  }
  this->TileScene = vtkSmartPointer<vtkMRMLScene>::New();
  if (this->SourceScene)
  {
    // Make the application default node settings available in the mirror scene. The node classes
    // that modules register in the source scene are registered in the mirror scene on demand (see
    // CreateMirrorNode): registering all of them (several hundred, each by instantiating the class)
    // would be the most expensive part of creating a mirror.
    this->SourceScene->CopyDefaultNodesToScene(this->TileScene);
  }
}

//----------------------------------------------------------------------------
vtkMRMLScene* vtkSequenceLightboxSceneMirror::GetTileScene()
{
  this->CreateTileScene();
  return this->TileScene;
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkSequenceLightboxSceneMirror::GetMirrorNode(const char* sourceNodeID)
{
  if (!sourceNodeID)
  {
    return nullptr;
  }
  std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(sourceNodeID);
  if (it == this->MirroredNodes.end())
  {
    return nullptr;
  }
  return it->second.MirrorNode;
}

//----------------------------------------------------------------------------
vtkMRMLAbstractViewNode* vtkSequenceLightboxSceneMirror::GetTileViewNode()
{
  if (!this->ReferenceViewNode)
  {
    return nullptr;
  }
  return vtkMRMLAbstractViewNode::SafeDownCast(this->GetMirrorNode(this->ReferenceViewNode->GetID()));
}

//----------------------------------------------------------------------------
vtkMRMLSliceCompositeNode* vtkSequenceLightboxSceneMirror::GetTileSliceCompositeNode()
{
  if (!this->ReferenceSliceCompositeNode)
  {
    return nullptr;
  }
  return vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMirrorNode(this->ReferenceSliceCompositeNode->GetID()));
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::UpdateReferenceViewRelatedNodes()
{
  this->ReferenceCameraNode = nullptr;
  this->ReferenceSliceCompositeNode = nullptr;
  if (!this->SourceScene || !this->ReferenceViewNode)
  {
    return;
  }
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(this->ReferenceViewNode);
  if (sliceNode)
  {
    this->ReferenceSliceCompositeNode = vtkMRMLSliceLogic::GetSliceCompositeNode(sliceNode);
    return;
  }
  vtkMRMLViewNode* viewNode = vtkMRMLViewNode::SafeDownCast(this->ReferenceViewNode);
  if (viewNode && viewNode->GetLayoutName())
  {
    this->ReferenceCameraNode = vtkMRMLViewLogic::GetCameraNode(this->SourceScene, viewNode->GetLayoutName());
  }
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::IsNodeMirrored(vtkMRMLNode* sourceNode)
{
  if (!sourceNode || !sourceNode->GetID())
  {
    return false;
  }

  // Sequences and their browsers are not rendered. The lightbox reads them directly from the
  // source scene, so mirroring them (and their potentially very large private scenes) would
  // be both expensive and useless.
  if (sourceNode->IsA("vtkMRMLSequenceNode") ||         //
      sourceNode->IsA("vtkMRMLSequenceBrowserNode") ||  //
      sourceNode->IsA("vtkMRMLStorageNode") ||          //
      sourceNode->IsA("vtkMRMLSubjectHierarchyNode") || //
      sourceNode->IsA("vtkMRMLSceneViewNode") ||        //
      sourceNode->IsA("vtkMRMLLayoutNode"))
  {
    return false;
  }

  // The interaction, selection and crosshair nodes of the application are mirrored, so that
  // the displayable managers of the tile behave like the ones of the regular views: they use
  // the same interaction mode (for example markup placement), the same active place node and
  // the same crosshair position. There is one of each of these nodes in the scene.
  if (sourceNode->IsA("vtkMRMLInteractionNode") || //
      sourceNode->IsA("vtkMRMLSelectionNode") ||   //
      sourceNode->IsA("vtkMRMLCrosshairNode"))
  {
    return this->SourceScene != nullptr && sourceNode == this->SourceScene->GetFirstNodeByClass(sourceNode->GetClassName());
  }

  // Only the reference view is mirrored: the tile renders that single view.
  if (sourceNode->IsA("vtkMRMLAbstractViewNode"))
  {
    return sourceNode == this->ReferenceViewNode.GetPointer();
  }
  if (sourceNode->IsA("vtkMRMLCameraNode"))
  {
    return sourceNode == this->ReferenceCameraNode.GetPointer();
  }
  if (sourceNode->IsA("vtkMRMLSliceCompositeNode"))
  {
    return sourceNode == this->ReferenceSliceCompositeNode.GetPointer();
  }

  // Slice models of the source scene views would show the slice planes of the main views.
  // Each tile creates its own slice model if it needs one.
  if (vtkMRMLSliceLogic::IsSliceModelNode(sourceNode))
  {
    return false;
  }
  vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(sourceNode);
  if (displayNode && vtkMRMLSliceLogic::IsSliceModelDisplayNode(displayNode))
  {
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::CollectReferencedNodes(vtkMRMLNode* sourceNode, std::set<std::string>& sourceNodeIDs)
{
  if (!this->IsNodeMirrored(sourceNode))
  {
    return;
  }
  if (!sourceNodeIDs.insert(sourceNode->GetID()).second)
  {
    // already visited
    return;
  }
  std::vector<std::string> roles;
  sourceNode->GetNodeReferenceRoles(roles);
  for (const std::string& role : roles)
  {
    std::vector<const char*> referencedNodeIDs;
    sourceNode->GetNodeReferenceIDs(role.c_str(), referencedNodeIDs);
    for (const char* referencedNodeID : referencedNodeIDs)
    {
      if (!referencedNodeID)
      {
        continue;
      }
      vtkMRMLNode* referencedNode = this->SourceScene->GetNodeByID(referencedNodeID);
      if (referencedNode)
      {
        this->CollectReferencedNodes(referencedNode, sourceNodeIDs);
      }
    }
  }

  // The color node of a display node is stored as a node ID string and not as a node
  // reference, therefore it is not found by the loop above. Without it the display pipeline
  // of the mirrored display node would have no lookup table.
  vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(sourceNode);
  if (displayNode && displayNode->GetColorNodeID())
  {
    vtkMRMLNode* colorNode = this->SourceScene->GetNodeByID(displayNode->GetColorNodeID());
    if (colorNode)
    {
      this->CollectReferencedNodes(colorNode, sourceNodeIDs);
    }
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::CollectNodesToMirror(std::set<std::string>& sourceNodeIDs, std::vector<vtkMRMLNode*>& candidateNodes)
{
  if (!this->SourceScene || !this->ReferenceViewNode)
  {
    return;
  }
  ++this->SourceSceneScanCount;

  // The view itself, and the nodes that define what it shows.
  this->CollectReferencedNodes(this->ReferenceViewNode, sourceNodeIDs);
  if (this->ReferenceSliceCompositeNode)
  {
    // Also pulls in the volumes of all layers (they are node references of the composite node).
    this->CollectReferencedNodes(this->ReferenceSliceCompositeNode, sourceNodeIDs);
  }
  if (this->ReferenceCameraNode)
  {
    this->CollectReferencedNodes(this->ReferenceCameraNode, sourceNodeIDs);
  }

  // The application state that the displayable managers of the tile rely on (see IsNodeMirrored).
  const char* applicationNodeClassNames[] = { "vtkMRMLInteractionNode", "vtkMRMLSelectionNode", "vtkMRMLCrosshairNode" };
  for (const char* applicationNodeClassName : applicationNodeClassNames)
  {
    vtkMRMLNode* applicationNode = this->SourceScene->GetFirstNodeByClass(applicationNodeClassName);
    if (applicationNode)
    {
      this->CollectReferencedNodes(applicationNode, sourceNodeIDs);
    }
  }

  // Everything that is visible in the reference view.
  const char* referenceViewNodeID = this->ReferenceViewNode->GetID();
  std::vector<vtkMRMLNode*> displayableNodes;
  this->SourceScene->GetNodesByClass("vtkMRMLDisplayableNode", displayableNodes);
  for (vtkMRMLNode* node : displayableNodes)
  {
    vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(node);
    if (!displayableNode || !this->IsNodeMirrored(displayableNode))
    {
      continue;
    }
    // Nodes are mirrored if they can be shown in the reference view, even if they are
    // currently hidden: the displayable managers of the tile check the visibility of the
    // display nodes themselves, exactly as they do in a regular view. Mirroring only the
    // visible nodes would also mean that this mirror does not observe the hidden ones, so
    // making a node visible would not be noticed.
    bool displayableInReferenceView = false;
    int numberOfDisplayNodes = displayableNode->GetNumberOfDisplayNodes();
    for (int displayNodeIndex = 0; displayNodeIndex < numberOfDisplayNodes; ++displayNodeIndex)
    {
      vtkMRMLDisplayNode* displayNode = displayableNode->GetNthDisplayNode(displayNodeIndex);
      if (displayNode && displayNode->IsDisplayableInView(referenceViewNodeID))
      {
        displayableInReferenceView = true;
        break;
      }
    }
    if (displayableInReferenceView)
    {
      this->CollectReferencedNodes(displayableNode, sourceNodeIDs);
    }
    else
    {
      // The node may become displayable in the reference view later (a display node is added,
      // or the reference view is added to the views of a display node), which is only noticed
      // if the node and its display nodes are observed.
      candidateNodes.push_back(displayableNode);
      for (int displayNodeIndex = 0; displayNodeIndex < numberOfDisplayNodes; ++displayNodeIndex)
      {
        vtkMRMLDisplayNode* displayNode = displayableNode->GetNthDisplayNode(displayNodeIndex);
        if (displayNode)
        {
          candidateNodes.push_back(displayNode);
        }
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::UpdateWatchedNodes(const std::vector<vtkMRMLNode*>& candidateNodes)
{
  // Watch the candidates that are not watched yet.
  std::set<std::string> candidateNodeIDs;
  for (vtkMRMLNode* candidateNode : candidateNodes)
  {
    if (!candidateNode || !candidateNode->GetID())
    {
      continue;
    }
    candidateNodeIDs.insert(candidateNode->GetID());
    WatchedNode& watchedNode = this->WatchedNodes[candidateNode->GetID()];
    if (watchedNode.Node == candidateNode)
    {
      continue;
    }
    if (watchedNode.Node)
    {
      watchedNode.Node->RemoveObserver(watchedNode.ObservationTag);
    }
    watchedNode.Node = candidateNode;
    // A modified event is enough: it is invoked when a display node is added to a displayable
    // node and when the view node IDs of a display node change.
    watchedNode.ObservationTag = candidateNode->AddObserver(vtkCommand::ModifiedEvent, this->SourceCallbackCommand);
  }
  // Stop watching the nodes that are not candidates anymore (they are mirrored now, or they
  // have been removed from the scene).
  for (std::map<std::string, WatchedNode>::iterator it = this->WatchedNodes.begin(); it != this->WatchedNodes.end();)
  {
    if (candidateNodeIDs.find(it->first) != candidateNodeIDs.end())
    {
      ++it;
      continue;
    }
    if (it->second.Node)
    {
      it->second.Node->RemoveObserver(it->second.ObservationTag);
    }
    it = this->WatchedNodes.erase(it);
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::RemoveAllWatchedNodeObservers()
{
  for (std::pair<const std::string, WatchedNode>& watchedNodeIt : this->WatchedNodes)
  {
    if (watchedNodeIt.second.Node)
    {
      watchedNodeIt.second.Node->RemoveObserver(watchedNodeIt.second.ObservationTag);
    }
  }
  this->WatchedNodes.clear();
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkSequenceLightboxSceneMirror::CreateMirrorNode(vtkMRMLNode* sourceNode)
{
  vtkMRMLScene* tileScene = this->GetTileScene();
  if (!tileScene || !sourceNode || !sourceNode->GetID())
  {
    return nullptr;
  }
  const char* sourceNodeID = sourceNode->GetID();
  vtkMRMLNode* mirrorNode = tileScene->GetNodeByID(sourceNodeID);
  if (!mirrorNode)
  {
    if (!tileScene->GetClassNameByTag(sourceNode->GetNodeTagName()))
    {
      // The class of the node (registered in the source scene by a module) is not known to the
      // mirror scene yet.
      vtkSmartPointer<vtkMRMLNode> nodeClassPrototype = vtkSmartPointer<vtkMRMLNode>::Take(sourceNode->CreateNodeInstance());
      tileScene->RegisterNodeClass(nodeClassPrototype);
    }
    // Keeping the source node ID makes all node references between mirrored nodes valid
    // in the mirror scene without any modification.
    mirrorNode = tileScene->AddNewNodeByClassWithID(sourceNode->GetClassName(), //
                                                    sourceNode->GetName() ? sourceNode->GetName() : "",
                                                    sourceNodeID);
    if (!mirrorNode)
    {
      vtkWarningMacro("vtkSequenceLightboxSceneMirror::CreateMirrorNode failed to create a mirror of " << sourceNodeID);
      return nullptr;
    }
  }
  // The singleton tag is not copied by CopyContent but several logics and displayable managers
  // find their nodes by it (it is the layout name of view, camera and slice composite nodes).
  if (sourceNode->GetSingletonTag())
  {
    mirrorNode->SetSingletonTag(sourceNode->GetSingletonTag());
  }
  return mirrorNode;
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::IsSelectedItem()
{
  vtkMRMLSequenceBrowserNode* browserNode = this->SequenceBrowserNode;
  return browserNode != nullptr && this->ItemNumber >= 0 && browserNode->GetSelectedItemNumber() == this->ItemNumber;
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkSequenceLightboxSceneMirror::GetContentSourceNode(vtkMRMLNode* sourceNode)
{
  vtkMRMLSequenceBrowserNode* browserNode = this->SequenceBrowserNode;
  if (!browserNode || !sourceNode || !browserNode->GetSequenceNode(sourceNode))
  {
    // Not a proxy node: the content is the node itself.
    return sourceNode;
  }
  vtkMRMLNode* dataNode = this->GetSequenceItemDataNode(sourceNode);
  if (dataNode && this->IsSelectedItem())
  {
    // The proxy node shows the mirrored item. If the proxy node has been modified since the
    // browser node updated it from the sequence then it may contain changes that have not been
    // saved into the sequence (yet): its content is used instead of the sequence item, so that
    // the tile shows exactly what the regular views show. Otherwise the sequence item is used,
    // which is what the tile showed before the item was selected: the tile does not have to be
    // updated at all when the selection changes (the proxy node is a deep copy of the item if the
    // changes of the proxy node are not saved, so copying it would make the tile recompute
    // everything it displays, for example the slice image).
    std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(sourceNode->GetID());
    bool proxyNodeModified = (it == this->MirroredNodes.end() || it->second.CleanProxyNodeMTime == 0 || sourceNode->GetMTime() > it->second.CleanProxyNodeMTime);
    if (proxyNodeModified)
    {
      return sourceNode;
    }
  }
  return dataNode;
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkSequenceLightboxSceneMirror::GetSequenceItemDataNode(vtkMRMLNode* sourceNode)
{
  vtkMRMLSequenceBrowserNode* browserNode = this->SequenceBrowserNode;
  if (!browserNode || !sourceNode)
  {
    return nullptr;
  }
  vtkMRMLSequenceNode* sequenceNode = browserNode->GetSequenceNode(sourceNode);
  if (!sequenceNode)
  {
    // Not a proxy node.
    return nullptr;
  }
  std::string indexValue;
  if (!this->GetSequenceItemIndexValue(indexValue))
  {
    return nullptr;
  }
  // The proxy node shows the previous item if the sequence has no item for the index value and
  // the missing item mode is "create from previous". In the other modes it shows an empty node
  // (or is hidden, or keeps its content), which the tile represents by hiding the node.
  bool exactMatchRequired = (browserNode->GetMissingItemMode(sequenceNode) != vtkMRMLSequenceBrowserNode::MissingItemCreateFromPrevious);
  vtkMRMLNode* dataNode = sequenceNode->GetDataNodeAtValue(indexValue, exactMatchRequired);
  if (!dataNode)
  {
    return nullptr;
  }
  if (std::strcmp(dataNode->GetClassName(), sourceNode->GetClassName()) != 0)
  {
    // The sequence contains a different node type than the proxy node, it cannot be displayed.
    return nullptr;
  }
  return dataNode;
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::DefinedControlPointsDiffer(vtkMRMLMarkupsNode* markupsNode, vtkMRMLMarkupsNode* referenceMarkupsNode)
{
  if (!markupsNode)
  {
    return false;
  }
  int numberOfDefinedControlPoints = markupsNode->GetNumberOfDefinedControlPoints();
  if (!referenceMarkupsNode)
  {
    return numberOfDefinedControlPoints > 0;
  }
  if (numberOfDefinedControlPoints != referenceMarkupsNode->GetNumberOfDefinedControlPoints())
  {
    return true;
  }
  int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();
  int numberOfReferenceControlPoints = referenceMarkupsNode->GetNumberOfControlPoints();
  int referenceControlPointIndex = 0;
  for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
  {
    if (markupsNode->GetNthControlPointPositionStatus(controlPointIndex) != vtkMRMLMarkupsNode::PositionDefined)
    {
      continue;
    }
    // Next defined control point of the reference node.
    while (referenceControlPointIndex < numberOfReferenceControlPoints
           && referenceMarkupsNode->GetNthControlPointPositionStatus(referenceControlPointIndex) != vtkMRMLMarkupsNode::PositionDefined)
    {
      ++referenceControlPointIndex;
    }
    if (referenceControlPointIndex >= numberOfReferenceControlPoints)
    {
      return true;
    }
    double position[3] = { 0.0, 0.0, 0.0 };
    double referencePosition[3] = { 0.0, 0.0, 0.0 };
    markupsNode->GetNthControlPointPosition(controlPointIndex, position);
    referenceMarkupsNode->GetNthControlPointPosition(referenceControlPointIndex, referencePosition);
    if (vtkMath::Distance2BetweenPoints(position, referencePosition) > 1e-6)
    {
      return true;
    }
    ++referenceControlPointIndex;
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::GetSequenceItemIndexValue(std::string& indexValue)
{
  vtkMRMLSequenceBrowserNode* browserNode = this->SequenceBrowserNode;
  vtkMRMLSequenceNode* masterSequenceNode = (browserNode ? browserNode->GetMasterSequenceNode() : nullptr);
  if (!masterSequenceNode)
  {
    return false;
  }
  if (this->ItemNumber < 0 || this->ItemNumber >= masterSequenceNode->GetNumberOfDataNodes())
  {
    return false;
  }
  // The master sequence defines the index values, exactly as when proxy nodes are updated.
  indexValue = masterSequenceNode->GetNthIndexValue(this->ItemNumber);
  return true;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::UpdateMirrorNode(vtkMRMLNode* sourceNode, vtkMRMLNode* mirrorNode)
{
  if (!sourceNode || !mirrorNode || !sourceNode->GetID())
  {
    return;
  }
  MirroredNode& mirroredNode = this->MirroredNodes[sourceNode->GetID()];

  mirroredNode.IsProxyNode = (this->SequenceBrowserNode != nullptr && this->SequenceBrowserNode->GetSequenceNode(sourceNode) != nullptr);
  vtkMRMLNode* contentSourceNode = this->GetContentSourceNode(sourceNode);
  mirroredNode.ItemMissing = (mirroredNode.IsProxyNode && contentSourceNode == nullptr);

  // The source node is observed, so a modified event of it is a reliable indication that it has
  // changed (some nodes, for example markups nodes, only invoke custom events when their content
  // changes, which do not update their modified time). It only matters if the content of the
  // mirror comes from the source node itself: a proxy node is modified on every step of the
  // sequence, which is irrelevant for the tiles that show the content of a sequence item. The
  // modified time of the content source is also checked, because a sequence item data node is
  // not observed (it only changes when the mirror writes into it).
  bool copyContent = this->FullUpdateNeeded                                                  //
                     || (mirroredNode.SourceModified && contentSourceNode == sourceNode)     //
                     || contentSourceNode != mirroredNode.LastContentSourceNode.GetPointer() //
                     || (contentSourceNode != nullptr && contentSourceNode->GetMTime() != mirroredNode.LastContentMTime);
  bool copyReferences = this->FullUpdateNeeded || mirroredNode.SourceModified || sourceNode->GetMTime() != mirroredNode.LastReferenceMTime;
  mirroredNode.SourceModified = false;

  // Display nodes are frequently modified without any change of their content (for example the
  // display node of a volume is modified whenever the volume gets new image data, which happens
  // for proxy nodes on every step of a sequence). Copying unchanged content would still modify
  // the mirror node, which makes the displayable managers of the tile rebuild what they display
  // (the slice image of a tile would be recomputed on every step, in every tile). The content of
  // a display node is fully described by its XML attributes, so the copy is skipped if they did
  // not change since the last copy. This is not done for other nodes: their bulk data (image,
  // mesh, control points, ...) is not part of their XML attributes.
  std::string contentXML;
  if (copyContent && contentSourceNode && !this->FullUpdateNeeded && vtkMRMLDisplayNode::SafeDownCast(contentSourceNode) //
      && contentSourceNode == mirroredNode.LastContentSourceNode.GetPointer())
  {
    contentXML = vtkSequenceLightboxSceneMirror::GetContentXML(contentSourceNode);
    if (contentXML == mirroredNode.LastContentXML)
    {
      copyContent = false;
    }
  }

  if (mirroredNode.IsProxyNode && !contentSourceNode && !mirroredNode.ContentIsDefault)
  {
    // The sequence has no item for the mirrored item (and the previous item is not shown): the
    // mirror node is reset to an empty node, as the proxy node is in the "create from default"
    // missing item mode. Keeping the previously mirrored content would be misleading (it is
    // hidden, but it would for example prevent placing the markups of the item, because the
    // mirrored markups node would already be complete).
    MRMLNodeModifyBlocker blocker(mirrorNode);
    vtkSmartPointer<vtkMRMLNode> defaultNode = vtkSmartPointer<vtkMRMLNode>::Take(mirrorNode->CreateNodeInstance());
    vtkSequenceLightboxSceneMirror::CopyNodeContent(mirrorNode, defaultNode);
    mirroredNode.ContentIsDefault = true;
  }

  if (copyContent && contentSourceNode)
  {
    MRMLNodeModifyBlocker blocker(mirrorNode);
    vtkSequenceLightboxSceneMirror::CopyNodeContent(mirrorNode, contentSourceNode);
    mirroredNode.ContentIsDefault = false;

    // View node IDs are not node references and are not copied by CopyContent.
    vtkMRMLDisplayNode* sourceDisplayNode = vtkMRMLDisplayNode::SafeDownCast(contentSourceNode);
    vtkMRMLDisplayNode* mirrorDisplayNode = vtkMRMLDisplayNode::SafeDownCast(mirrorNode);
    if (sourceDisplayNode && mirrorDisplayNode)
    {
      // Only if they differ: removing the view node IDs modifies the node even if there are none.
      int numberOfViewNodeIDs = sourceDisplayNode->GetNumberOfViewNodeIDs();
      bool viewNodeIDsDiffer = (numberOfViewNodeIDs != mirrorDisplayNode->GetNumberOfViewNodeIDs());
      for (int viewNodeIndex = 0; viewNodeIndex < numberOfViewNodeIDs && !viewNodeIDsDiffer; ++viewNodeIndex)
      {
        const char* sourceViewNodeID = sourceDisplayNode->GetNthViewNodeID(viewNodeIndex);
        const char* mirrorViewNodeID = mirrorDisplayNode->GetNthViewNodeID(viewNodeIndex);
        viewNodeIDsDiffer = (sourceViewNodeID == nullptr) != (mirrorViewNodeID == nullptr) //
                            || (sourceViewNodeID && mirrorViewNodeID && std::strcmp(sourceViewNodeID, mirrorViewNodeID) != 0);
      }
      if (viewNodeIDsDiffer)
      {
        mirrorDisplayNode->RemoveAllViewNodeIDs();
        for (int viewNodeIndex = 0; viewNodeIndex < numberOfViewNodeIDs; ++viewNodeIndex)
        {
          mirrorDisplayNode->AddViewNodeID(sourceDisplayNode->GetNthViewNodeID(viewNodeIndex));
        }
      }
      if (contentXML.empty())
      {
        contentXML = vtkSequenceLightboxSceneMirror::GetContentXML(contentSourceNode);
      }
      mirroredNode.LastContentXML = contentXML;
    }
  }

  if (copyReferences && vtkSequenceLightboxSceneMirror::NodeReferencesDiffer(mirrorNode, sourceNode))
  {
    MRMLNodeModifyBlocker blocker(mirrorNode);
    // References are copied from the source node (not from the sequence item data node):
    // display nodes and parent transforms of a proxy node are the ones of the proxy node.
    // CopyReferences removes and re-adds all references, which makes the displayable managers
    // of the tile rebuild their pipelines, so it is only called if the references differ.
    mirrorNode->CopyReferences(sourceNode);
  }

  mirroredNode.LastContentSourceNode = contentSourceNode;
  mirroredNode.LastContentMTime = (contentSourceNode ? contentSourceNode->GetMTime() : 0);
  mirroredNode.LastReferenceMTime = sourceNode->GetMTime();
  vtkMRMLSequenceNode* sequenceNode = (mirroredNode.IsProxyNode ? this->SequenceBrowserNode->GetSequenceNode(sourceNode) : nullptr);
  mirroredNode.LastSequenceMTime = (sequenceNode ? sequenceNode->GetMTime() : 0);
  if (copyReferences)
  {
    mirroredNode.StructureSignature = vtkSequenceLightboxSceneMirror::ComputeStructureSignature(sourceNode);
  }
}

//----------------------------------------------------------------------------
std::string vtkSequenceLightboxSceneMirror::GetContentXML(vtkMRMLNode* node)
{
  if (!node)
  {
    return "";
  }
  std::ostringstream xmlStream;
  node->WriteXML(xmlStream, 0);
  return xmlStream.str();
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::UpdateProxyNodeVisibility()
{
  for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
  {
    MirroredNode& mirroredNode = mirroredNodeIt.second;
    if (!mirroredNode.IsProxyNode)
    {
      continue;
    }
    vtkMRMLDisplayableNode* mirrorDisplayableNode = vtkMRMLDisplayableNode::SafeDownCast(mirroredNode.MirrorNode);
    if (!mirrorDisplayableNode)
    {
      continue;
    }
    // The visibility of a mirrored proxy node is derived from the source node and from the
    // availability of the item, and it is applied on every update. Hiding the node only when
    // the item is missing would be permanent, because the content of the mirror is not copied
    // again while the source node is unchanged.
    int numberOfDisplayNodes = mirrorDisplayableNode->GetNumberOfDisplayNodes();
    for (int displayNodeIndex = 0; displayNodeIndex < numberOfDisplayNodes; ++displayNodeIndex)
    {
      vtkMRMLDisplayNode* mirrorDisplayNode = mirrorDisplayableNode->GetNthDisplayNode(displayNodeIndex);
      if (!mirrorDisplayNode || !mirrorDisplayNode->GetID())
      {
        continue;
      }
      int visibility = 0;
      if (!mirroredNode.ItemMissing)
      {
        // Mirrored nodes keep their node ID, so the source display node has the same ID.
        vtkMRMLDisplayNode* sourceDisplayNode = vtkMRMLDisplayNode::SafeDownCast(this->SourceScene ? this->SourceScene->GetNodeByID(mirrorDisplayNode->GetID()) : nullptr);
        visibility = (sourceDisplayNode ? sourceDisplayNode->GetVisibility() : 1);
      }
      mirrorDisplayNode->SetVisibility(visibility);
    }
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::AddSourceNodeObserver(vtkMRMLNode* sourceNode)
{
  if (!sourceNode || !sourceNode->GetID())
  {
    return;
  }
  MirroredNode& mirroredNode = this->MirroredNodes[sourceNode->GetID()];
  if (!mirroredNode.ObservationTags.empty())
  {
    return;
  }
  mirroredNode.ObservationTags.push_back(sourceNode->AddObserver(vtkCommand::ModifiedEvent, this->SourceCallbackCommand));
  vtkIntArray* contentModifiedEvents = sourceNode->GetContentModifiedEvents();
  if (contentModifiedEvents)
  {
    for (int eventIndex = 0; eventIndex < contentModifiedEvents->GetNumberOfTuples(); ++eventIndex)
    {
      mirroredNode.ObservationTags.push_back(sourceNode->AddObserver(contentModifiedEvents->GetValue(eventIndex), this->SourceCallbackCommand));
    }
  }

  // The mirror node is observed as well, to detect the modifications that have to be written
  // back to the source scene, and the requests to show a context menu that have to be forwarded
  // to the application.
  vtkMRMLNode* mirrorNode = mirroredNode.MirrorNode;
  if (mirrorNode && mirroredNode.MirrorObservationTags.empty())
  {
    mirroredNode.MirrorObservationTags.push_back(mirrorNode->AddObserver(vtkCommand::ModifiedEvent, this->MirrorCallbackCommand));
    if (vtkMRMLDisplayNode::SafeDownCast(mirrorNode))
    {
      mirroredNode.MirrorObservationTags.push_back(mirrorNode->AddObserver(vtkMRMLDisplayNode::MenuEvent, this->MirrorCallbackCommand));
    }
    if (vtkMRMLInteractionNode::SafeDownCast(mirrorNode))
    {
      mirroredNode.MirrorObservationTags.push_back(mirrorNode->AddObserver(vtkMRMLInteractionNode::ShowViewContextMenuEvent, this->MirrorCallbackCommand));
    }
    vtkIntArray* mirrorContentModifiedEvents = mirrorNode->GetContentModifiedEvents();
    if (mirrorContentModifiedEvents)
    {
      for (int eventIndex = 0; eventIndex < mirrorContentModifiedEvents->GetNumberOfTuples(); ++eventIndex)
      {
        mirroredNode.MirrorObservationTags.push_back(mirrorNode->AddObserver(mirrorContentModifiedEvents->GetValue(eventIndex), this->MirrorCallbackCommand));
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::RemoveSourceNodeObserver(vtkMRMLNode* sourceNode)
{
  if (!sourceNode || !sourceNode->GetID())
  {
    return;
  }
  std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(sourceNode->GetID());
  if (it == this->MirroredNodes.end())
  {
    return;
  }
  for (unsigned long tag : it->second.ObservationTags)
  {
    sourceNode->RemoveObserver(tag);
  }
  it->second.ObservationTags.clear();
  if (it->second.MirrorNode)
  {
    for (unsigned long tag : it->second.MirrorObservationTags)
    {
      it->second.MirrorNode->RemoveObserver(tag);
    }
  }
  it->second.MirrorObservationTags.clear();
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::RemoveAllSourceNodeObservers()
{
  for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
  {
    MirroredNode& mirroredNode = mirroredNodeIt.second;
    if (mirroredNode.SourceNode)
    {
      for (unsigned long tag : mirroredNode.ObservationTags)
      {
        mirroredNode.SourceNode->RemoveObserver(tag);
      }
    }
    mirroredNode.ObservationTags.clear();
    if (mirroredNode.MirrorNode)
    {
      for (unsigned long tag : mirroredNode.MirrorObservationTags)
      {
        mirroredNode.MirrorNode->RemoveObserver(tag);
      }
    }
    mirroredNode.MirrorObservationTags.clear();
  }
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::Clear()
{
  this->RemoveAllSourceNodeObservers();
  this->RemoveAllWatchedNodeObservers();
  if (this->TileScene)
  {
    this->TileScene->StartState(vtkMRMLScene::BatchProcessState);
    for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
    {
      vtkMRMLNode* mirrorNode = mirroredNodeIt.second.MirrorNode;
      if (mirrorNode && this->TileScene->IsNodePresent(mirrorNode))
      {
        this->TileScene->RemoveNode(mirrorNode);
      }
    }
    this->TileScene->EndState(vtkMRMLScene::BatchProcessState);
  }
  this->MirroredNodes.clear();
  this->PendingWriteBackNodeIDs.clear();
  this->FullUpdateNeeded = true;
  this->SourceStructureModified = true;
  this->ItemNumberModified = true;
}

//----------------------------------------------------------------------------
bool vtkSequenceLightboxSceneMirror::IsAnyMirroredNodeModified()
{
  if (this->SequenceBrowserNode && this->SequenceBrowserNode->GetMTime() != this->LastBrowserNodeMTime)
  {
    return true;
  }
  for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
  {
    MirroredNode& mirroredNode = mirroredNodeIt.second;
    if (mirroredNode.SourceNode && mirroredNode.SourceNode->GetMTime() != mirroredNode.LastReferenceMTime)
    {
      return true;
    }
    if (mirroredNode.LastContentSourceNode && mirroredNode.LastContentSourceNode->GetMTime() != mirroredNode.LastContentMTime)
    {
      return true;
    }
    if (mirroredNode.IsProxyNode && mirroredNode.SourceNode && this->SequenceBrowserNode)
    {
      vtkMRMLSequenceNode* sequenceNode = this->SequenceBrowserNode->GetSequenceNode(mirroredNode.SourceNode);
      if (sequenceNode && sequenceNode->GetMTime() != mirroredNode.LastSequenceMTime)
      {
        return true;
      }
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkSequenceLightboxSceneMirror::Update()
{
  if (this->UpdateInProgress)
  {
    return;
  }
  this->UpdateInProgress = true;

  if (!this->SourceScene || !this->ReferenceViewNode)
  {
    this->Clear();
    this->UpdateInProgress = false;
    return;
  }

  // Scanning the source scene and rebuilding the mirror scene is only needed if something may
  // have changed. All tiles mirror the same source scene, so without these checks a single
  // change of a source node would make every tile scan the whole source scene once per tile.
  // The content source of a proxy node mirror depends on whether the mirrored item is the
  // selected one, so a change of the selection is handled like a change of the item number.
  int selectedItemNumber = (this->SequenceBrowserNode ? this->SequenceBrowserNode->GetSelectedItemNumber() : -1);
  if (selectedItemNumber != this->LastSelectedItemNumber)
  {
    this->LastSelectedItemNumber = selectedItemNumber;
    this->ItemNumberModified = true;
    // The browser node has updated its proxy nodes from the newly selected item (it does so
    // immediately when the selection changes): the proxy nodes are clean now, any later
    // modification of a proxy node is a change that may not be saved into the sequence yet.
    // If a proxy node is not newer than the browser node then it has not been updated yet (for
    // example the browser node is observed by this mirror before it is observed by the logic that
    // updates the proxy nodes): the proxy node is then treated as modified until the next
    // selection change, which is always safe.
    vtkMTimeType browserNodeMTime = (this->SequenceBrowserNode ? this->SequenceBrowserNode->GetMTime() : 0);
    for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
    {
      MirroredNode& mirroredNode = mirroredNodeIt.second;
      if (!mirroredNode.IsProxyNode || !mirroredNode.SourceNode)
      {
        continue;
      }
      vtkMTimeType proxyNodeMTime = mirroredNode.SourceNode->GetMTime();
      mirroredNode.CleanProxyNodeMTime = (proxyNodeMTime > browserNodeMTime ? proxyNodeMTime : 0);
    }
  }

  bool collectNodes = this->FullUpdateNeeded || this->SourceStructureModified;
  if (!collectNodes && !this->ItemNumberModified && !this->MirroredNodeContentModified && !this->IsAnyMirroredNodeModified())
  {
    // Nothing changed since the last update.
    this->UpdateInProgress = false;
    return;
  }

  // The composite node of a slice view and the camera node of a 3D view are found by the layout
  // name of the view, so they can change without any mirrored node changing its references.
  vtkMRMLNode* previousReferenceCameraNode = this->ReferenceCameraNode;
  vtkMRMLNode* previousReferenceSliceCompositeNode = this->ReferenceSliceCompositeNode;
  this->UpdateReferenceViewRelatedNodes();
  if (this->ReferenceCameraNode != previousReferenceCameraNode || this->ReferenceSliceCompositeNode != previousReferenceSliceCompositeNode)
  {
    collectNodes = true;
  }

  std::set<std::string> sourceNodeIDs;
  if (collectNodes)
  {
    std::vector<vtkMRMLNode*> candidateNodes;
    this->CollectNodesToMirror(sourceNodeIDs, candidateNodes);
    this->UpdateWatchedNodes(candidateNodes);
  }
  else
  {
    // The set of mirrored nodes did not change, only their content has to be synchronized.
    for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
    {
      sourceNodeIDs.insert(mirroredNodeIt.first);
    }
  }

  vtkMRMLScene* tileScene = this->GetTileScene();

  // Mirrors of nodes that are not needed anymore.
  std::vector<std::string> removedSourceNodeIDs;
  for (std::pair<const std::string, MirroredNode>& mirroredNodeIt : this->MirroredNodes)
  {
    if (sourceNodeIDs.find(mirroredNodeIt.first) == sourceNodeIDs.end())
    {
      removedSourceNodeIDs.push_back(mirroredNodeIt.first);
    }
  }
  // Nodes that are not mirrored yet.
  std::vector<std::string> addedSourceNodeIDs;
  for (const std::string& sourceNodeID : sourceNodeIDs)
  {
    if (this->MirroredNodes.find(sourceNodeID) == this->MirroredNodes.end())
    {
      addedSourceNodeIDs.push_back(sourceNodeID);
    }
  }

  // Adding and removing nodes is batched, so that the displayable managers of the tile rebuild
  // their pipelines once. Copying content is not batched: it only modifies nodes that already
  // exist, and a batch process would make every displayable manager of the tile update
  // everything it shows, even when a single node changed.
  bool structureChanged = !removedSourceNodeIDs.empty() || !addedSourceNodeIDs.empty();
  if (structureChanged)
  {
    tileScene->StartState(vtkMRMLScene::BatchProcessState);
  }

  for (const std::string& removedSourceNodeID : removedSourceNodeIDs)
  {
    MirroredNode& mirroredNode = this->MirroredNodes[removedSourceNodeID];
    if (mirroredNode.SourceNode)
    {
      for (unsigned long tag : mirroredNode.ObservationTags)
      {
        mirroredNode.SourceNode->RemoveObserver(tag);
      }
    }
    if (mirroredNode.MirrorNode)
    {
      for (unsigned long tag : mirroredNode.MirrorObservationTags)
      {
        mirroredNode.MirrorNode->RemoveObserver(tag);
      }
    }
    if (mirroredNode.MirrorNode && tileScene->IsNodePresent(mirroredNode.MirrorNode))
    {
      tileScene->RemoveNode(mirroredNode.MirrorNode);
    }
    this->MirroredNodes.erase(removedSourceNodeID);
  }

  // Create missing mirrors. All nodes are created before any content is copied, so that
  // node references between them can be resolved immediately.
  for (const std::string& sourceNodeID : addedSourceNodeIDs)
  {
    vtkMRMLNode* sourceNode = this->SourceScene->GetNodeByID(sourceNodeID);
    if (!sourceNode)
    {
      continue;
    }
    vtkMRMLNode* mirrorNode = this->CreateMirrorNode(sourceNode);
    if (!mirrorNode)
    {
      continue;
    }
    MirroredNode& mirroredNode = this->MirroredNodes[sourceNodeID];
    mirroredNode.MirrorNode = mirrorNode;
    mirroredNode.SourceNode = sourceNode;
    this->AddSourceNodeObserver(sourceNode);
  }

  // Copy content and references.
  for (const std::string& sourceNodeID : sourceNodeIDs)
  {
    std::map<std::string, MirroredNode>::iterator it = this->MirroredNodes.find(sourceNodeID);
    if (it == this->MirroredNodes.end())
    {
      continue;
    }
    this->UpdateMirrorNode(it->second.SourceNode, it->second.MirrorNode);
  }

  // A tile is a single view: linking would try to propagate its properties to the other
  // (non-existing) views of the mirror scene and would make the tile follow the main views.
  vtkMRMLSliceCompositeNode* tileCompositeNode = this->GetTileSliceCompositeNode();
  if (tileCompositeNode)
  {
    tileCompositeNode->SetLinkedControl(0);
    tileCompositeNode->SetHotLinkedControl(0);
  }

  this->UpdateProxyNodeVisibility();

  if (structureChanged)
  {
    tileScene->EndState(vtkMRMLScene::BatchProcessState);
  }

  this->FullUpdateNeeded = false;
  this->SourceStructureModified = false;
  this->ItemNumberModified = false;
  this->MirroredNodeContentModified = false;
  this->LastBrowserNodeMTime = (this->SequenceBrowserNode ? this->SequenceBrowserNode->GetMTime() : 0);
  this->UpdateInProgress = false;
}
