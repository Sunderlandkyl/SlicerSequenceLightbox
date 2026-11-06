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
#include "vtkMRMLSequenceLightboxViewNode.h"
#include "vtkSlicerSequenceLightboxLogic.h"
#include "vtkSlicerSequencesLogic.h"

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSequenceNode.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLViewNode.h"

// VTK includes
#include <vtkNew.h>

// STD includes
#include <algorithm>

namespace
{

//----------------------------------------------------------------------------
int TestItemMapping()
{
  vtkNew<vtkMRMLScene> scene;
  // Registers the sequence browser and the lightbox view node classes in the scene.
  vtkNew<vtkSlicerSequencesLogic> sequencesLogic;
  sequencesLogic->SetMRMLScene(scene);
  vtkNew<vtkSlicerSequenceLightboxLogic> lightboxLogic;
  lightboxLogic->SetMRMLScene(scene);

  vtkMRMLSequenceNode* sequenceNode = vtkMRMLSequenceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceNode"));
  CHECK_NOT_NULL(sequenceNode);
  for (int itemIndex = 0; itemIndex < 5; ++itemIndex)
  {
    vtkNew<vtkMRMLSliceNode> dataNode;
    sequenceNode->SetDataNodeAtValue(dataNode, std::to_string(itemIndex));
  }

  vtkMRMLSequenceBrowserNode* browserNode = vtkMRMLSequenceBrowserNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceBrowserNode"));
  CHECK_NOT_NULL(browserNode);
  browserNode->SetAndObserveMasterSequenceNodeID(sequenceNode->GetID());
  CHECK_INT(browserNode->GetNumberOfItems(), 5);

  vtkMRMLSequenceLightboxViewNode* viewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceLightboxViewNode"));
  CHECK_NOT_NULL(viewNode);

  // Without a browser node no item can be displayed.
  CHECK_INT(viewNode->GetItemNumberForTile(0), -1);

  viewNode->SetAndObserveSequenceBrowserNodeID(browserNode->GetID());
  CHECK_POINTER(viewNode->GetSequenceBrowserNode(), browserNode);

  viewNode->SetRows(2);
  viewNode->SetColumns(3);
  CHECK_INT(viewNode->GetNumberOfTiles(), 6);

  // Fixed first item.
  viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed);
  viewNode->SetStartItemNumber(0);
  viewNode->SetItemStep(1);
  for (int tileIndex = 0; tileIndex < 5; ++tileIndex)
  {
    CHECK_INT(viewNode->GetItemNumberForTile(tileIndex), tileIndex);
  }
  // There are only 5 items, so the 6th tile is empty.
  CHECK_INT(viewNode->GetItemNumberForTile(5), -1);
  // Out of range tiles are empty, too.
  CHECK_INT(viewNode->GetItemNumberForTile(-1), -1);
  CHECK_INT(viewNode->GetItemNumberForTile(6), -1);

  // Item step.
  viewNode->SetItemStep(2);
  CHECK_INT(viewNode->GetItemNumberForTile(0), 0);
  CHECK_INT(viewNode->GetItemNumberForTile(1), 2);
  CHECK_INT(viewNode->GetItemNumberForTile(2), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(3), -1);
  CHECK_INT(viewNode->GetTileForItemNumber(2), 1);
  CHECK_INT(viewNode->GetTileForItemNumber(1), -1);

  // The first tile follows the selected item of the browser node.
  viewNode->SetItemStep(1);
  viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionFollowSelected);
  browserNode->SetSelectedItemNumber(2);
  CHECK_INT(viewNode->GetItemNumberForTile(0), 2);
  CHECK_INT(viewNode->GetItemNumberForTile(2), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(3), -1);

  // The selected item is shown in the center tile (tile 3 of 6), the tiles before it show the
  // previous items. Tiles that would show an item before the first one are empty.
  viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected);
  browserNode->SetSelectedItemNumber(2);
  CHECK_INT(viewNode->GetFirstItemNumber(), -1);
  CHECK_INT(viewNode->GetItemNumberForTile(0), -1);
  CHECK_INT(viewNode->GetItemNumberForTile(1), 0);
  CHECK_INT(viewNode->GetItemNumberForTile(3), 2);
  CHECK_INT(viewNode->GetItemNumberForTile(5), 4);
  CHECK_INT(viewNode->GetTileForItemNumber(2), 3);
  // The selected item stays in the center tile when the selection changes.
  browserNode->SetSelectedItemNumber(4);
  CHECK_INT(viewNode->GetItemNumberForTile(3), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(4), -1);
  CHECK_INT(viewNode->GetItemNumberForTile(0), 1);
  // Item step applies around the selected item as well.
  viewNode->SetItemStep(2);
  CHECK_INT(viewNode->GetItemNumberForTile(3), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(2), 2);
  CHECK_INT(viewNode->GetItemNumberForTile(1), 0);
  viewNode->SetItemStep(1);
  browserNode->SetSelectedItemNumber(2);

  // An explicit list of items.
  viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom);
  CHECK_INT(viewNode->GetFirstItemNumber(), -1);
  viewNode->SetCustomItemNumbersFromString("4, 0, 2");
  CHECK_INT(static_cast<int>(viewNode->GetCustomItemNumbers().size()), 3);
  CHECK_INT(viewNode->GetItemNumberForTile(0), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(1), 0);
  CHECK_INT(viewNode->GetItemNumberForTile(2), 2);
  // Tiles without an item in the list are empty.
  CHECK_INT(viewNode->GetItemNumberForTile(3), -1);
  CHECK_INT(viewNode->GetTileForItemNumber(0), 1);

  // Ranges, and item numbers that do not exist in the sequence.
  viewNode->SetCustomItemNumbersFromString("1-3, 99");
  CHECK_INT(static_cast<int>(viewNode->GetCustomItemNumbers().size()), 4);
  CHECK_INT(viewNode->GetItemNumberForTile(0), 1);
  CHECK_INT(viewNode->GetItemNumberForTile(2), 3);
  // The sequence has 5 items, so item 99 cannot be displayed.
  CHECK_INT(viewNode->GetItemNumberForTile(3), -1);

  // Invalid values are ignored, so that the list can be edited while it is incomplete.
  viewNode->SetCustomItemNumbersFromString("0, , not a number, 2");
  CHECK_INT(static_cast<int>(viewNode->GetCustomItemNumbers().size()), 2);
  CHECK_INT(viewNode->GetItemNumberForTile(1), 2);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestCustomItemNumbersString()
{
  vtkNew<vtkMRMLSequenceLightboxViewNode> viewNode;

  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "");

  viewNode->SetCustomItemNumbersFromString("3");
  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "3");

  // Consecutive item numbers are written as a range, but only if it is shorter.
  viewNode->SetCustomItemNumbersFromString("7-9");
  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "7-9");

  viewNode->SetCustomItemNumbersFromString("4,5");
  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "4, 5");

  viewNode->SetCustomItemNumbersFromString("0, 3, 7, 8, 9");
  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "0, 3, 7-9");

  // The order of the items is preserved.
  viewNode->SetCustomItemNumbersFromString("9, 1");
  CHECK_STD_STRING(viewNode->GetCustomItemNumbersAsString(), "9, 1");

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestReferences()
{
  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkSlicerSequencesLogic> sequencesLogic;
  sequencesLogic->SetMRMLScene(scene);
  vtkNew<vtkSlicerSequenceLightboxLogic> lightboxLogic;
  lightboxLogic->SetMRMLScene(scene);

  vtkMRMLSequenceLightboxViewNode* viewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSequenceLightboxViewNode"));
  CHECK_NOT_NULL(viewNode);
  CHECK_NULL(viewNode->GetSequenceBrowserNode());
  CHECK_NULL(viewNode->GetReferenceViewNode());
  CHECK_BOOL(viewNode->IsThreeDMode(), false);

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceNode"));
  viewNode->SetAndObserveReferenceViewNodeID(sliceNode->GetID());
  CHECK_POINTER(viewNode->GetReferenceViewNode(), sliceNode);
  CHECK_BOOL(viewNode->IsThreeDMode(), false);

  vtkMRMLViewNode* threeDViewNode = vtkMRMLViewNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLViewNode"));
  viewNode->SetAndObserveReferenceViewNodeID(threeDViewNode->GetID());
  CHECK_POINTER(viewNode->GetReferenceViewNode(), threeDViewNode);
  CHECK_BOOL(viewNode->IsThreeDMode(), true);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestItemSelectionModeString()
{
  CHECK_STD_STRING(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeAsString(vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed), "fixed");
  CHECK_INT(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeFromString("followSelected"), vtkMRMLSequenceLightboxViewNode::ItemSelectionFollowSelected);
  CHECK_STD_STRING(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeAsString(vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected), "centerSelected");
  CHECK_INT(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeFromString("centerSelected"), vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected);
  CHECK_INT(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeFromString("invalid mode name"), -1);
  CHECK_INT(vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeFromString(nullptr), -1);
  return EXIT_SUCCESS;
}

} // namespace

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
int TestFitGridToItems()
{
  vtkNew<vtkMRMLSequenceLightboxViewNode> node;
  // 6 items in a wide view: a single row.
  CHECK_BOOL(node->FitGridToItems(6, 1200.0, 200.0), true);
  CHECK_INT(node->GetRows(), 1);
  CHECK_INT(node->GetColumns(), 6);
  // 6 items in a square view: 2 rows of 3 (tiles of 333x300) beat 3 rows of 2 (300x333)...
  CHECK_BOOL(node->FitGridToItems(6, 1000.0, 1000.0), true);
  CHECK_INT(node->GetRows() * node->GetColumns() >= 6, 1);
  CHECK_INT(std::min(node->GetRows(), node->GetColumns()), 2);
  CHECK_INT(std::max(node->GetRows(), node->GetColumns()), 3);
  // 26 items in a 4:3 view: 6 columns and 5 rows (tiles of 200x200), or similar.
  CHECK_BOOL(node->FitGridToItems(26, 1200.0, 1000.0), true);
  CHECK_INT(node->GetRows() * node->GetColumns() >= 26, 1);
  CHECK_INT(node->GetRows() * node->GetColumns() <= 30, 1);
  // Too many items: the largest grid.
  CHECK_BOOL(node->FitGridToItems(1000, 1000.0, 1000.0), false);
  CHECK_INT(node->GetRows(), vtkMRMLSequenceLightboxViewNode::MaximumGridSize);
  CHECK_INT(node->GetColumns(), vtkMRMLSequenceLightboxViewNode::MaximumGridSize);
  // Degenerate inputs.
  CHECK_BOOL(node->FitGridToItems(0, 0.0, 0.0), true);
  CHECK_INT(node->GetRows(), 1);
  CHECK_INT(node->GetColumns(), 1);
  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int vtkMRMLSequenceLightboxViewNodeTest1(int, char*[])
{
  vtkNew<vtkMRMLSequenceLightboxViewNode> node1;
  EXERCISE_ALL_BASIC_MRML_METHODS(node1);
  // Tiles are interactive by default.
  CHECK_BOOL(node1->GetInteractiveTiles(), true);

  CHECK_EXIT_SUCCESS(TestItemMapping());
  CHECK_EXIT_SUCCESS(TestReferences());
  CHECK_EXIT_SUCCESS(TestItemSelectionModeString());
  CHECK_EXIT_SUCCESS(TestCustomItemNumbersString());
  CHECK_EXIT_SUCCESS(TestFitGridToItems());

  std::cout << "vtkMRMLSequenceLightboxViewNodeTest1 completed successfully" << std::endl;
  return EXIT_SUCCESS;
}
