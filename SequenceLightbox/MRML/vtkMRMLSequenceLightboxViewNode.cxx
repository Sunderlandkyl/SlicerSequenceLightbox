/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Sequences MRML includes
#include "vtkMRMLSequenceLightboxViewNode.h"
#include "vtkMRMLSequenceBrowserNode.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <algorithm>
#include <cstring>
#include <sstream>

const char* vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeReferenceRole = "sequenceBrowser";
const char* vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeReferenceMRMLAttributeName = "sequenceBrowserNodeRef";
const char* vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeReferenceRole = "referenceView";
const char* vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeReferenceMRMLAttributeName = "referenceViewNodeRef";

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSequenceLightboxViewNode);

//----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode::vtkMRMLSequenceLightboxViewNode()
{
  this->TypeDisplayName = vtkMRMLTr("vtkMRMLSequenceLightboxViewNode", "Sequence Lightbox View");

  // The background is visible in the gaps between the tiles (and in the place of empty tiles),
  // so it differs from the black background of the slice tiles.
  this->BackgroundColor[0] = 0.25;
  this->BackgroundColor[1] = 0.25;
  this->BackgroundColor[2] = 0.25;
  this->BackgroundColor2[0] = 0.25;
  this->BackgroundColor2[1] = 0.25;
  this->BackgroundColor2[2] = 0.25;

  vtkNew<vtkIntArray> browserEvents;
  browserEvents->InsertNextValue(vtkCommand::ModifiedEvent);
  browserEvents->InsertNextValue(vtkMRMLSequenceBrowserNode::SequenceNodeModifiedEvent);
  browserEvents->InsertNextValue(vtkMRMLSequenceBrowserNode::ProxyNodeModifiedEvent);
  browserEvents->InsertNextValue(vtkMRMLSequenceBrowserNode::IndexDisplayFormatModifiedEvent);
  this->AddNodeReferenceRole(this->GetSequenceBrowserNodeReferenceRole(), //
                             this->GetSequenceBrowserNodeReferenceMRMLAttributeName(),
                             browserEvents.GetPointer());

  vtkNew<vtkIntArray> viewEvents;
  viewEvents->InsertNextValue(vtkCommand::ModifiedEvent);
  this->AddNodeReferenceRole(this->GetReferenceViewNodeReferenceRole(), //
                             this->GetReferenceViewNodeReferenceMRMLAttributeName(),
                             viewEvents.GetPointer());
}

//----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode::~vtkMRMLSequenceLightboxViewNode() = default;

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLIntMacro(rows, Rows);
  vtkMRMLWriteXMLIntMacro(columns, Columns);
  vtkMRMLWriteXMLIntMacro(tileSpacing, TileSpacing);
  vtkMRMLWriteXMLIntMacro(itemStep, ItemStep);
  vtkMRMLWriteXMLEnumMacro(itemSelectionMode, ItemSelectionMode);
  vtkMRMLWriteXMLIntMacro(startItemNumber, StartItemNumber);
  vtkMRMLWriteXMLStdIntVectorMacro(customItemNumbers, CustomItemNumbers, std::vector<int>);
  vtkMRMLWriteXMLBooleanMacro(showItemLabels, ShowItemLabels);
  vtkMRMLWriteXMLBooleanMacro(highlightSelectedItem, HighlightSelectedItem);
  vtkMRMLWriteXMLBooleanMacro(interactiveTiles, InteractiveTiles);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLIntMacro(rows, Rows);
  vtkMRMLReadXMLIntMacro(columns, Columns);
  vtkMRMLReadXMLIntMacro(tileSpacing, TileSpacing);
  vtkMRMLReadXMLIntMacro(itemStep, ItemStep);
  vtkMRMLReadXMLEnumMacro(itemSelectionMode, ItemSelectionMode);
  vtkMRMLReadXMLIntMacro(startItemNumber, StartItemNumber);
  vtkMRMLReadXMLStdIntVectorMacro(customItemNumbers, CustomItemNumbers, std::vector<int>);
  vtkMRMLReadXMLBooleanMacro(showItemLabels, ShowItemLabels);
  vtkMRMLReadXMLBooleanMacro(highlightSelectedItem, HighlightSelectedItem);
  vtkMRMLReadXMLBooleanMacro(interactiveTiles, InteractiveTiles);
  vtkMRMLReadXMLEndMacro();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyIntMacro(Rows);
  vtkMRMLCopyIntMacro(Columns);
  vtkMRMLCopyIntMacro(TileSpacing);
  vtkMRMLCopyIntMacro(ItemStep);
  vtkMRMLCopyEnumMacro(ItemSelectionMode);
  vtkMRMLCopyIntMacro(StartItemNumber);
  vtkMRMLCopyStdIntVectorMacro(CustomItemNumbers);
  vtkMRMLCopyBooleanMacro(ShowItemLabels);
  vtkMRMLCopyBooleanMacro(HighlightSelectedItem);
  vtkMRMLCopyBooleanMacro(InteractiveTiles);
  vtkMRMLCopyEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintIntMacro(Rows);
  vtkMRMLPrintIntMacro(Columns);
  vtkMRMLPrintIntMacro(TileSpacing);
  vtkMRMLPrintIntMacro(ItemStep);
  vtkMRMLPrintEnumMacro(ItemSelectionMode);
  vtkMRMLPrintIntMacro(StartItemNumber);
  vtkMRMLPrintStdIntVectorMacro(CustomItemNumbers, std::vector<int>);
  vtkMRMLPrintBooleanMacro(ShowItemLabels);
  vtkMRMLPrintBooleanMacro(HighlightSelectedItem);
  vtkMRMLPrintBooleanMacro(InteractiveTiles);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetSequenceBrowserNodeReferenceRole()
{
  return vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeReferenceRole;
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetSequenceBrowserNodeReferenceMRMLAttributeName()
{
  return vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeReferenceMRMLAttributeName;
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetReferenceViewNodeReferenceRole()
{
  return vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeReferenceRole;
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetReferenceViewNodeReferenceMRMLAttributeName()
{
  return vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeReferenceMRMLAttributeName;
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::SetAndObserveSequenceBrowserNodeID(const char* sequenceBrowserNodeID)
{
  this->SetAndObserveNodeReferenceID(this->GetSequenceBrowserNodeReferenceRole(), sequenceBrowserNodeID);
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetSequenceBrowserNodeID()
{
  return this->GetNodeReferenceID(this->GetSequenceBrowserNodeReferenceRole());
}

//----------------------------------------------------------------------------
vtkMRMLSequenceBrowserNode* vtkMRMLSequenceLightboxViewNode::GetSequenceBrowserNode()
{
  return vtkMRMLSequenceBrowserNode::SafeDownCast(this->GetNodeReference(this->GetSequenceBrowserNodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::SetAndObserveReferenceViewNodeID(const char* referenceViewNodeID)
{
  this->SetAndObserveNodeReferenceID(this->GetReferenceViewNodeReferenceRole(), referenceViewNodeID);
}

//----------------------------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetReferenceViewNodeID()
{
  return this->GetNodeReferenceID(this->GetReferenceViewNodeReferenceRole());
}

//----------------------------------------------------------------------------
vtkMRMLAbstractViewNode* vtkMRMLSequenceLightboxViewNode::GetReferenceViewNode()
{
  return vtkMRMLAbstractViewNode::SafeDownCast(this->GetNodeReference(this->GetReferenceViewNodeReferenceRole()));
}

//----------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxViewNode::IsThreeDMode()
{
  vtkMRMLAbstractViewNode* referenceViewNode = this->GetReferenceViewNode();
  return referenceViewNode != nullptr && referenceViewNode->IsA("vtkMRMLViewNode");
}

//----------------------------------------------------------------------------
int vtkMRMLSequenceLightboxViewNode::GetNumberOfTiles()
{
  return this->Rows * this->Columns;
}

//----------------------------------------------------------------------------
bool vtkMRMLSequenceLightboxViewNode::FitGridToItems(int numberOfItems, double viewWidth, double viewHeight)
{
  numberOfItems = std::max(1, numberOfItems);
  if (viewWidth <= 0.0 || viewHeight <= 0.0)
  {
    viewWidth = 1.0;
    viewHeight = 1.0;
  }
  // Try every number of columns and keep the grid whose tiles are the largest (a tile is as
  // large as the smaller of its width and its height, as if it were square).
  int bestRows = 1;
  int bestColumns = 1;
  double bestTileSize = -1.0;
  for (int columns = 1; columns <= MaximumGridSize; ++columns)
  {
    int rows = (numberOfItems + columns - 1) / columns;
    if (rows > MaximumGridSize)
    {
      continue;
    }
    double tileSize = std::min(viewWidth / columns, viewHeight / rows);
    if (tileSize > bestTileSize)
    {
      bestTileSize = tileSize;
      bestRows = rows;
      bestColumns = columns;
    }
  }
  if (bestTileSize < 0.0)
  {
    // Not all items fit: use the largest grid.
    bestRows = MaximumGridSize;
    bestColumns = MaximumGridSize;
  }
  int wasModifying = this->StartModify();
  this->SetRows(bestRows);
  this->SetColumns(bestColumns);
  this->EndModify(wasModifying);
  return bestRows * bestColumns >= numberOfItems;
}

//----------------------------------------------------------------------------
int vtkMRMLSequenceLightboxViewNode::GetItemNumberForTile(int tileIndex)
{
  if (tileIndex < 0 || tileIndex >= this->GetNumberOfTiles())
  {
    return -1;
  }
  vtkMRMLSequenceBrowserNode* browserNode = this->GetSequenceBrowserNode();
  if (!browserNode)
  {
    return -1;
  }
  int numberOfItems = browserNode->GetNumberOfItems();
  if (numberOfItems <= 0)
  {
    return -1;
  }
  if (this->ItemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom)
  {
    if (tileIndex >= static_cast<int>(this->CustomItemNumbers.size()))
    {
      // There is no item for this tile.
      return -1;
    }
    int customItemNumber = this->CustomItemNumbers[tileIndex];
    if (customItemNumber < 0 || customItemNumber >= numberOfItems)
    {
      return -1;
    }
    return customItemNumber;
  }

  int itemNumber = this->GetFirstItemNumber() + tileIndex * this->ItemStep;
  if (itemNumber < 0 || itemNumber >= numberOfItems)
  {
    return -1;
  }
  return itemNumber;
}

//----------------------------------------------------------------------------
int vtkMRMLSequenceLightboxViewNode::GetFirstItemNumber()
{
  vtkMRMLSequenceBrowserNode* browserNode = this->GetSequenceBrowserNode();
  if (!browserNode || this->ItemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom)
  {
    return -1;
  }
  if (this->ItemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed)
  {
    return this->StartItemNumber;
  }
  int selectedItemNumber = browserNode->GetSelectedItemNumber();
  if (selectedItemNumber < 0)
  {
    selectedItemNumber = 0;
  }
  if (this->ItemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected)
  {
    // The selected item is shown in the center tile (for an even number of tiles, in the first
    // tile of the second half). The item number of the first tile may be negative: those tiles
    // are empty, so that the selected item stays in the same tile.
    int centerTileIndex = this->GetNumberOfTiles() / 2;
    return selectedItemNumber - centerTileIndex * this->ItemStep;
  }
  return selectedItemNumber;
}

//----------------------------------------------------------------------------
int vtkMRMLSequenceLightboxViewNode::GetTileForItemNumber(int itemNumber)
{
  int numberOfTiles = this->GetNumberOfTiles();
  for (int tileIndex = 0; tileIndex < numberOfTiles; ++tileIndex)
  {
    if (this->GetItemNumberForTile(tileIndex) == itemNumber)
    {
      return tileIndex;
    }
  }
  return -1;
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::InvokeEventForReferenceRole(const char* referenceRole, vtkMRMLNode* referencedNode)
{
  if (!referenceRole)
  {
    return;
  }
  if (std::strcmp(referenceRole, this->GetSequenceBrowserNodeReferenceRole()) == 0)
  {
    this->InvokeEvent(vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeModifiedEvent, referencedNode);
  }
  else if (std::strcmp(referenceRole, this->GetReferenceViewNodeReferenceRole()) == 0)
  {
    this->InvokeEvent(vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, referencedNode);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::OnNodeReferenceAdded(vtkMRMLNodeReference* reference)
{
  this->Superclass::OnNodeReferenceAdded(reference);
  this->InvokeEventForReferenceRole(reference->GetReferenceRole(), reference->GetReferencedNode());
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::OnNodeReferenceModified(vtkMRMLNodeReference* reference)
{
  this->Superclass::OnNodeReferenceModified(reference);
  this->InvokeEventForReferenceRole(reference->GetReferenceRole(), reference->GetReferencedNode());
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::OnNodeReferenceRemoved(vtkMRMLNodeReference* reference)
{
  this->Superclass::OnNodeReferenceRemoved(reference);
  this->InvokeEventForReferenceRole(reference->GetReferenceRole(), reference->GetReferencedNode());
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData)
{
  Superclass::ProcessMRMLEvents(caller, event, callData);

  vtkMRMLNode* callerNode = vtkMRMLNode::SafeDownCast(caller);
  if (!callerNode)
  {
    return;
  }

  vtkMRMLSequenceBrowserNode* browserNode = this->GetSequenceBrowserNode();
  if (browserNode != nullptr && browserNode == callerNode)
  {
    this->InvokeEvent(vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeModifiedEvent, browserNode);
    return;
  }

  vtkMRMLAbstractViewNode* referenceViewNode = this->GetReferenceViewNode();
  if (referenceViewNode != nullptr && referenceViewNode == callerNode)
  {
    this->InvokeEvent(vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, referenceViewNode);
    return;
  }
}

//-----------------------------------------------------------
const char* vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeAsString(int id)
{
  switch (id)
  {
    case ItemSelectionFixed: return "fixed";
    case ItemSelectionFollowSelected: return "followSelected";
    case ItemSelectionCustom: return "custom";
    case ItemSelectionCenterSelected: return "centerSelected";
    default:
      // invalid id
      return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLSequenceLightboxViewNode::GetItemSelectionModeFromString(const char* name)
{
  if (name == nullptr)
  {
    // invalid name
    return -1;
  }
  for (int ii = 0; ii < NumberOfItemSelectionModes; ii++)
  {
    if (std::strcmp(name, GetItemSelectionModeAsString(ii)) == 0)
    {
      // found a matching name
      return ii;
    }
  }
  // unknown name
  return -1;
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::SetCustomItemNumbers(const std::vector<int>& itemNumbers)
{
  if (this->CustomItemNumbers == itemNumbers)
  {
    return;
  }
  this->CustomItemNumbers = itemNumbers;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewNode::SetCustomItemNumbersFromString(const std::string& itemNumbers)
{
  std::vector<int> parsedItemNumbers;
  std::stringstream itemNumbersStream(itemNumbers);
  std::string token;
  while (std::getline(itemNumbersStream, token, ',') && parsedItemNumbers.size() < MaximumNumberOfCustomItems)
  {
    // A token is either a single item number ("3") or a range of item numbers ("7-9").
    std::size_t separatorPosition = token.find('-', token.find_first_not_of(" \t"));
    try
    {
      if (separatorPosition == std::string::npos)
      {
        parsedItemNumbers.push_back(std::stoi(token));
      }
      else
      {
        int rangeStart = std::stoi(token.substr(0, separatorPosition));
        int rangeEnd = std::stoi(token.substr(separatorPosition + 1));
        int step = (rangeStart <= rangeEnd ? 1 : -1);
        for (int itemNumber = rangeStart; itemNumber != rangeEnd + step && parsedItemNumbers.size() < MaximumNumberOfCustomItems; itemNumber += step)
        {
          parsedItemNumbers.push_back(itemNumber);
        }
      }
    }
    catch (...)
    {
      // Ignore tokens that are not item numbers, so that the list can be edited by the user
      // while it is incomplete.
      continue;
    }
  }
  this->SetCustomItemNumbers(parsedItemNumbers);
}

//----------------------------------------------------------------------------
std::string vtkMRMLSequenceLightboxViewNode::GetCustomItemNumbersAsString()
{
  std::stringstream itemNumbersStream;
  std::size_t itemIndex = 0;
  while (itemIndex < this->CustomItemNumbers.size())
  {
    // Write consecutive item numbers as a range.
    std::size_t rangeEndIndex = itemIndex;
    while (rangeEndIndex + 1 < this->CustomItemNumbers.size() //
           && this->CustomItemNumbers[rangeEndIndex + 1] == this->CustomItemNumbers[rangeEndIndex] + 1)
    {
      ++rangeEndIndex;
    }
    if (itemIndex > 0)
    {
      itemNumbersStream << ", ";
    }
    itemNumbersStream << this->CustomItemNumbers[itemIndex];
    if (rangeEndIndex > itemIndex + 1)
    {
      itemNumbersStream << "-" << this->CustomItemNumbers[rangeEndIndex];
    }
    else if (rangeEndIndex == itemIndex + 1)
    {
      itemNumbersStream << ", " << this->CustomItemNumbers[rangeEndIndex];
    }
    itemIndex = rangeEndIndex + 1;
  }
  return itemNumbersStream.str();
}
