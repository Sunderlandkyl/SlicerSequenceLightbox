/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Sequences includes
#include "vtkMRMLSequenceBrowserNode.h"
#include "vtkSequenceLightboxSceneMirror.h"
#include "vtkSlicerSequenceLightboxLogic.h"
#include "vtkSlicerSequencesLogic.h"

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLDisplayNode.h"
#include "vtkMRMLInteractionNode.h"
#include "vtkMRMLModelNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSequenceNode.h"
#include "vtkMRMLSliceCompositeNode.h"
#include "vtkMRMLSliceNode.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPolyData.h>

namespace
{

const int NUMBER_OF_ITEMS = 3;

//----------------------------------------------------------------------------
/// Create a sequence of small volumes, each filled with a different value.
vtkMRMLSequenceNode* CreateVolumeSequence(vtkMRMLScene* scene)
{
  vtkMRMLSequenceNode* sequenceNode = vtkMRMLSequenceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceNode"));
  if (!sequenceNode)
  {
    return nullptr;
  }
  for (int itemIndex = 0; itemIndex < NUMBER_OF_ITEMS; ++itemIndex)
  {
    vtkNew<vtkMRMLScalarVolumeNode> volumeNode;
    vtkNew<vtkImageData> imageData;
    imageData->SetDimensions(4, 4, 4);
    imageData->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    unsigned char* voxels = static_cast<unsigned char*>(imageData->GetScalarPointer());
    for (vtkIdType voxelIndex = 0; voxelIndex < 4 * 4 * 4; ++voxelIndex)
    {
      voxels[voxelIndex] = static_cast<unsigned char>(10 * (itemIndex + 1));
    }
    volumeNode->SetAndObserveImageData(imageData);
    sequenceNode->SetDataNodeAtValue(volumeNode, std::to_string(itemIndex));
  }
  return sequenceNode;
}

//----------------------------------------------------------------------------
/// Modifications of the mirror scene are written back to the source scene while write-back is
/// enabled: to the reference view (so that all tiles follow), and to the sequence item that a
/// proxy node mirror shows.
int TestWriteBack()
{
  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkSlicerSequencesLogic> sequencesLogic;
  sequencesLogic->SetMRMLScene(scene);
  vtkNew<vtkSlicerSequenceLightboxLogic> lightboxLogic;
  lightboxLogic->SetMRMLScene(scene);

  vtkMRMLSequenceNode* sequenceNode = CreateVolumeSequence(scene);
  CHECK_NOT_NULL(sequenceNode);
  vtkMRMLSequenceBrowserNode* browserNode = vtkMRMLSequenceBrowserNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceBrowserNode"));
  CHECK_NOT_NULL(browserNode);
  browserNode->SetAndObserveMasterSequenceNodeID(sequenceNode->GetID());
  sequencesLogic->UpdateProxyNodesFromSequences(browserNode);
  vtkMRMLScalarVolumeNode* proxyNode = vtkMRMLScalarVolumeNode::SafeDownCast(browserNode->GetProxyNode(sequenceNode));
  CHECK_NOT_NULL(proxyNode);
  CHECK_INT(browserNode->GetSelectedItemNumber(), 0);

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceNode"));
  CHECK_NOT_NULL(sliceNode);
  sliceNode->SetLayoutName("Red");
  vtkMRMLSliceCompositeNode* compositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceCompositeNode"));
  CHECK_NOT_NULL(compositeNode);
  compositeNode->SetLayoutName("Red");
  compositeNode->SetBackgroundVolumeID(proxyNode->GetID());
  compositeNode->SetLinkedControl(1);

  // The interaction node of the application determines what a click does (for example placing
  // a markup). It has to be mirrored so that the tiles behave like the regular views.
  vtkMRMLInteractionNode* interactionNode = vtkMRMLInteractionNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLInteractionNode"));
  CHECK_NOT_NULL(interactionNode);
  interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);

  vtkNew<vtkSequenceLightboxSceneMirror> mirror;
  mirror->SetSourceScene(scene);
  mirror->SetSequenceBrowserNode(browserNode);
  mirror->SetReferenceViewNode(sliceNode);
  mirror->SetItemNumber(1);
  mirror->Update();
  vtkMRMLScene* tileScene = mirror->GetTileScene();
  CHECK_NOT_NULL(tileScene);

  vtkMRMLInteractionNode* tileInteractionNode = vtkMRMLInteractionNode::SafeDownCast(tileScene->GetNodeByID(interactionNode->GetID()));
  CHECK_NOT_NULL(tileInteractionNode);
  CHECK_INT(tileInteractionNode->GetCurrentInteractionMode(), vtkMRMLInteractionNode::ViewTransform);
  // Interaction nodes do not implement CopyContent, they must still be synchronized.
  interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::Place);
  mirror->Update();
  CHECK_INT(tileInteractionNode->GetCurrentInteractionMode(), vtkMRMLInteractionNode::Place);

  vtkMRMLSliceNode* tileSliceNode = vtkMRMLSliceNode::SafeDownCast(mirror->GetTileViewNode());
  CHECK_NOT_NULL(tileSliceNode);
  vtkMRMLSliceCompositeNode* tileCompositeNode = mirror->GetTileSliceCompositeNode();
  CHECK_NOT_NULL(tileCompositeNode);
  vtkMRMLScalarVolumeNode* tileProxyNode = vtkMRMLScalarVolumeNode::SafeDownCast(tileScene->GetNodeByID(proxyNode->GetID()));
  CHECK_NOT_NULL(tileProxyNode);

  // Write-back is disabled by default: the mirror scene is read-only.
  CHECK_BOOL(mirror->GetWriteBackEnabled(), false);
  double sliceOffset = sliceNode->GetSliceOffset();
  tileSliceNode->SetSliceOffset(sliceOffset + 5.0);
  mirror->WriteBackModifiedNodes();
  CHECK_DOUBLE_TOLERANCE(sliceNode->GetSliceOffset(), sliceOffset, 1e-6);
  CHECK_INT(static_cast<int>(mirror->GetWriteBackCount()), 0);
  // The next update of the source overwrites the local change.
  sliceNode->SetSliceOffset(sliceOffset + 1.0);
  mirror->Update();
  CHECK_DOUBLE_TOLERANCE(tileSliceNode->GetSliceOffset(), sliceOffset + 1.0, 1e-6);

  // Changes of the view are written back to the reference view (so that every tile follows).
  // Disabling write-back writes back the pending modifications.
  mirror->SetWriteBackEnabled(true);
  tileSliceNode->SetSliceOffset(sliceOffset + 7.0);
  CHECK_INT(static_cast<int>(mirror->GetWriteBackCount()), 0);
  mirror->SetWriteBackEnabled(false);
  CHECK_INT(static_cast<int>(mirror->GetWriteBackCount()), 1);
  CHECK_DOUBLE_TOLERANCE(sliceNode->GetSliceOffset(), sliceOffset + 7.0, 1e-6);
  // The source is now identical to the mirror, so the next update does not copy anything back.
  unsigned long scanCount = mirror->GetSourceSceneScanCount();
  mirror->Update();
  CHECK_DOUBLE_TOLERANCE(tileSliceNode->GetSliceOffset(), sliceOffset + 7.0, 1e-6);
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount));

  // Slice view linking is disabled in the mirror scene (a tile is a single view). Writing back
  // the composite node must not disable linking in the source scene.
  mirror->SetWriteBackEnabled(true);
  tileCompositeNode->SetForegroundOpacity(0.25);
  mirror->SetWriteBackEnabled(false);
  CHECK_DOUBLE_TOLERANCE(compositeNode->GetForegroundOpacity(), 0.25, 1e-6);
  CHECK_INT(compositeNode->GetLinkedControl(), 1);
  CHECK_INT(tileCompositeNode->GetLinkedControl(), 0);

  // Changes of the interaction mode made in the tile (for example when placement ends) are
  // applied to the application.
  mirror->SetWriteBackEnabled(true);
  tileInteractionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
  mirror->SetWriteBackEnabled(false);
  CHECK_INT(interactionNode->GetCurrentInteractionMode(), vtkMRMLInteractionNode::ViewTransform);

  // Changes of a proxy node mirror are stored in the sequence item that the tile shows (item 1),
  // not in the proxy node, which shows the selected item (item 0).
  vtkMRMLScalarVolumeNode* itemNode1 = vtkMRMLScalarVolumeNode::SafeDownCast(sequenceNode->GetNthDataNode(1));
  CHECK_NOT_NULL(itemNode1);
  double spacing[3] = { 1.0, 1.0, 1.0 };
  itemNode1->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 1.0, 1e-6);
  mirror->SetWriteBackEnabled(true);
  tileProxyNode->SetSpacing(2.0, 2.0, 2.0);
  mirror->SetWriteBackEnabled(false);
  itemNode1->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 2.0, 1e-6);
  proxyNode->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 1.0, 1e-6);
  // The sequence has unsaved changes now.
  CHECK_BOOL(sequenceNode->GetModifiedSinceRead(), true);

  // When the tile shows the selected item then the tile keeps displaying the sequence item (the
  // proxy node was just updated from it, so they are identical and nothing has to be copied)
  // until the proxy node is modified: it may then contain unsaved changes, which the tile must
  // display. Changes made in the tile are written to the proxy node as well.
  browserNode->SetSelectedItemNumber(1);
  sequencesLogic->UpdateProxyNodesFromSequences(browserNode);
  mirror->Update();
  CHECK_POINTER(tileProxyNode->GetImageData(), itemNode1->GetImageData());
  proxyNode->SetSpacing(2.5, 2.5, 2.5);
  mirror->Update();
  CHECK_POINTER(tileProxyNode->GetImageData(), proxyNode->GetImageData());
  tileProxyNode->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 2.5, 1e-6);
  mirror->SetWriteBackEnabled(true);
  tileProxyNode->SetSpacing(3.0, 3.0, 3.0);
  mirror->SetWriteBackEnabled(false);
  proxyNode->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 3.0, 1e-6);
  itemNode1->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 3.0, 1e-6);

  // A tile that shows an item that does not exist has nothing to write to.
  mirror->SetItemNumber(NUMBER_OF_ITEMS + 10);
  mirror->Update();
  unsigned long writeBackCount = mirror->GetWriteBackCount();
  mirror->SetWriteBackEnabled(true);
  tileProxyNode->SetSpacing(4.0, 4.0, 4.0);
  mirror->SetWriteBackEnabled(false);
  CHECK_INT(static_cast<int>(mirror->GetWriteBackCount()), static_cast<int>(writeBackCount));
  proxyNode->GetSpacing(spacing);
  CHECK_DOUBLE_TOLERANCE(spacing[0], 3.0, 1e-6);

  return EXIT_SUCCESS;
}

} // namespace

//----------------------------------------------------------------------------
int vtkSequenceLightboxSceneMirrorTest1(int, char*[])
{
  CHECK_EXIT_SUCCESS(TestWriteBack());

  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkSlicerSequencesLogic> sequencesLogic;
  sequencesLogic->SetMRMLScene(scene);
  vtkNew<vtkSlicerSequenceLightboxLogic> lightboxLogic;
  lightboxLogic->SetMRMLScene(scene);

  vtkMRMLSequenceNode* sequenceNode = CreateVolumeSequence(scene);
  CHECK_NOT_NULL(sequenceNode);
  CHECK_INT(sequenceNode->GetNumberOfDataNodes(), NUMBER_OF_ITEMS);

  vtkMRMLSequenceBrowserNode* browserNode = vtkMRMLSequenceBrowserNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceBrowserNode"));
  CHECK_NOT_NULL(browserNode);
  browserNode->SetAndObserveMasterSequenceNodeID(sequenceNode->GetID());
  sequencesLogic->UpdateProxyNodesFromSequences(browserNode);

  vtkMRMLScalarVolumeNode* proxyNode = vtkMRMLScalarVolumeNode::SafeDownCast(browserNode->GetProxyNode(sequenceNode));
  CHECK_NOT_NULL(proxyNode);

  // A slice view that shows the proxy node as background.
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceNode"));
  CHECK_NOT_NULL(sliceNode);
  sliceNode->SetLayoutName("Red");
  vtkMRMLSliceCompositeNode* compositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceCompositeNode"));
  CHECK_NOT_NULL(compositeNode);
  compositeNode->SetLayoutName("Red");
  compositeNode->SetBackgroundVolumeID(proxyNode->GetID());
  compositeNode->SetLinkedControl(1);

  // A model that is not part of the sequence.
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLModelNode"));
  CHECK_NOT_NULL(modelNode);
  vtkNew<vtkPolyData> polyData;
  modelNode->SetAndObservePolyData(polyData);
  modelNode->CreateDefaultDisplayNodes();

  vtkNew<vtkSequenceLightboxSceneMirror> mirror;
  mirror->SetSourceScene(scene);
  mirror->SetSequenceBrowserNode(browserNode);
  mirror->SetReferenceViewNode(sliceNode);
  mirror->SetItemNumber(1);
  mirror->Update();

  vtkMRMLScene* tileScene = mirror->GetTileScene();
  CHECK_NOT_NULL(tileScene);
  CHECK_BOOL(tileScene != scene.GetPointer(), true);

  // Nodes that are needed to render the reference view are mirrored, keeping their node ID.
  CHECK_NOT_NULL(tileScene->GetNodeByID(proxyNode->GetID()));
  CHECK_NOT_NULL(tileScene->GetNodeByID(sliceNode->GetID()));
  CHECK_NOT_NULL(tileScene->GetNodeByID(compositeNode->GetID()));
  CHECK_NOT_NULL(tileScene->GetNodeByID(modelNode->GetID()));
  CHECK_POINTER(mirror->GetTileViewNode(), tileScene->GetNodeByID(sliceNode->GetID()));

  // The layout name is needed by the logics that find their nodes by it.
  vtkMRMLSliceNode* tileSliceNode = vtkMRMLSliceNode::SafeDownCast(mirror->GetTileViewNode());
  CHECK_NOT_NULL(tileSliceNode);
  CHECK_STRING(tileSliceNode->GetLayoutName(), "Red");

  // A tile is a single view: linking must not be propagated to it.
  vtkMRMLSliceCompositeNode* tileCompositeNode = mirror->GetTileSliceCompositeNode();
  CHECK_NOT_NULL(tileCompositeNode);
  CHECK_INT(tileCompositeNode->GetLinkedControl(), 0);
  CHECK_STRING(tileCompositeNode->GetBackgroundVolumeID(), proxyNode->GetID());

  // Sequences and browser nodes are not rendered, so they are not mirrored.
  CHECK_NULL(tileScene->GetNodeByID(sequenceNode->GetID()));
  CHECK_NULL(tileScene->GetNodeByID(browserNode->GetID()));

  // The proxy node of the tile contains the data of the requested item, and shares the image
  // data with the sequence (mirroring must not copy bulk data).
  vtkMRMLScalarVolumeNode* tileProxyNode = vtkMRMLScalarVolumeNode::SafeDownCast(tileScene->GetNodeByID(proxyNode->GetID()));
  CHECK_NOT_NULL(tileProxyNode);
  vtkMRMLScalarVolumeNode* itemNode = vtkMRMLScalarVolumeNode::SafeDownCast(sequenceNode->GetNthDataNode(1));
  CHECK_NOT_NULL(itemNode);
  CHECK_POINTER(tileProxyNode->GetImageData(), itemNode->GetImageData());

  // The static model is shared with the source scene as well.
  vtkMRMLModelNode* tileModelNode = vtkMRMLModelNode::SafeDownCast(tileScene->GetNodeByID(modelNode->GetID()));
  CHECK_NOT_NULL(tileModelNode);
  CHECK_POINTER(tileModelNode->GetPolyData(), modelNode->GetPolyData());

  // Changing the item only changes the content of the proxy node.
  mirror->SetItemNumber(2);
  mirror->Update();
  CHECK_POINTER(vtkMRMLScalarVolumeNode::SafeDownCast(tileScene->GetNodeByID(proxyNode->GetID())), tileProxyNode);
  vtkMRMLScalarVolumeNode* itemNode2 = vtkMRMLScalarVolumeNode::SafeDownCast(sequenceNode->GetNthDataNode(2));
  CHECK_NOT_NULL(itemNode2);
  CHECK_POINTER(tileProxyNode->GetImageData(), itemNode2->GetImageData());

  // An item that does not exist hides the proxy node instead of showing stale content.
  mirror->SetItemNumber(NUMBER_OF_ITEMS + 10);
  mirror->Update();
  vtkMRMLDisplayNode* tileProxyDisplayNode = tileProxyNode->GetDisplayNode();
  if (tileProxyDisplayNode)
  {
    CHECK_INT(tileProxyDisplayNode->GetVisibility(), 0);
  }

  // An update only scans the source scene when the set of nodes to mirror may have changed.
  // Every tile of a lightbox mirrors the same source scene and every mirror is updated whenever
  // any of them reports a change, so an update that always rescanned the scene would make the
  // cost of a change grow with the square of the number of tiles.
  mirror->SetItemNumber(0);
  mirror->Update();
  unsigned long scanCount = mirror->GetSourceSceneScanCount();
  for (int updateIndex = 0; updateIndex < 10; ++updateIndex)
  {
    mirror->Update();
  }
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount));

  // Showing another item does not need the source scene to be scanned either.
  mirror->SetItemNumber(1);
  mirror->Update();
  CHECK_POINTER(tileProxyNode->GetImageData(), vtkMRMLScalarVolumeNode::SafeDownCast(sequenceNode->GetNthDataNode(1))->GetImageData());
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount));

  // A change of a mirrored node is applied without scanning the scene: the node references of
  // the slice node did not change, so the set of nodes to mirror cannot have changed either.
  sliceNode->SetSliceOffset(sliceNode->GetSliceOffset() + 10.0);
  mirror->Update();
  CHECK_DOUBLE_TOLERANCE(tileSliceNode->GetSliceOffset(), sliceNode->GetSliceOffset(), 1e-3);
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount));
  // Changing a node reference (the background volume of the view) does scan the scene.
  compositeNode->SetBackgroundVolumeID(nullptr);
  mirror->Update();
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount) + 1);
  compositeNode->SetBackgroundVolumeID(proxyNode->GetID());
  mirror->Update();
  CHECK_STRING(tileCompositeNode->GetBackgroundVolumeID(), proxyNode->GetID());
  scanCount = mirror->GetSourceSceneScanCount();
  for (int updateIndex = 0; updateIndex < 10; ++updateIndex)
  {
    mirror->Update();
  }
  CHECK_INT(static_cast<int>(mirror->GetSourceSceneScanCount()), static_cast<int>(scanCount));

  // Removing a node from the source scene removes its mirror.
  mirror->SetItemNumber(0);
  scene->RemoveNode(modelNode);
  mirror->Update();
  CHECK_NULL(tileScene->GetNodeByID("vtkMRMLModelNode1"));

  // Clearing releases every mirrored node.
  mirror->Clear();
  CHECK_NULL(tileScene->GetNodeByID(proxyNode->GetID()));
  CHECK_NULL(mirror->GetTileViewNode());

  std::cout << "vtkSequenceLightboxSceneMirrorTest1 completed successfully" << std::endl;
  return EXIT_SUCCESS;
}
