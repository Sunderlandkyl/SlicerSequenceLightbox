/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Qt includes
#include <QDebug>

// STD includes
#include <string>
#include <vector>

// Sequences includes
#include "qMRMLLayoutSequenceLightboxViewFactory.h"
#include "qMRMLSequenceLightboxWidget.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// qMRML includes
#include <qMRMLLayoutManager.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSequenceBrowserNode.h>
#include <vtkMRMLSliceNode.h>

//------------------------------------------------------------------------------
qMRMLLayoutSequenceLightboxViewFactory::qMRMLLayoutSequenceLightboxViewFactory(QObject* parent)
  : Superclass(parent)
{
}

//------------------------------------------------------------------------------
qMRMLLayoutSequenceLightboxViewFactory::~qMRMLLayoutSequenceLightboxViewFactory() = default;

//------------------------------------------------------------------------------
QString qMRMLLayoutSequenceLightboxViewFactory::viewClassName() const
{
  return "vtkMRMLSequenceLightboxViewNode";
}

//------------------------------------------------------------------------------
void qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(vtkMRMLScene* scene)
{
  if (!scene)
  {
    return;
  }
  std::vector<vtkMRMLNode*> viewNodes;
  scene->GetNodesByClass("vtkMRMLSequenceLightboxViewNode", viewNodes);
  for (vtkMRMLNode* node : viewNodes)
  {
    qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(scene, vtkMRMLSequenceLightboxViewNode::SafeDownCast(node));
  }
}

//------------------------------------------------------------------------------
void qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(vtkMRMLScene* scene, vtkMRMLSequenceLightboxViewNode* viewNode)
{
  if (!scene || !viewNode)
  {
    return;
  }
  if (!viewNode->GetSequenceBrowserNode())
  {
    // Any browser node is a better default than none. The user can change it in the controller
    // bar of the view.
    vtkMRMLNode* browserNode = scene->GetFirstNodeByClass("vtkMRMLSequenceBrowserNode");
    if (browserNode)
    {
      viewNode->SetAndObserveSequenceBrowserNodeID(browserNode->GetID());
    }
  }
  if (!viewNode->GetReferenceViewNode())
  {
    // Both the red slice view and the first 3D view always exist (they are created by the
    // layout logic). A lightbox that is placed next to a 3D view in its layout follows that
    // 3D view, every other lightbox follows the red slice view.
    const char* layoutName = viewNode->GetLayoutName();
    bool threeDMode = (layoutName != nullptr && std::string(layoutName).find("3D") != std::string::npos);
    vtkMRMLNode* referenceViewNode = nullptr;
    if (threeDMode)
    {
      referenceViewNode = scene->GetSingletonNode("1", "vtkMRMLViewNode");
    }
    if (!referenceViewNode)
    {
      referenceViewNode = scene->GetSingletonNode("Red", "vtkMRMLSliceNode");
    }
    if (referenceViewNode)
    {
      viewNode->SetAndObserveReferenceViewNodeID(referenceViewNode->GetID());
    }
  }
}

//------------------------------------------------------------------------------
bool qMRMLLayoutSequenceLightboxViewFactory::isTimelineLayoutName(const char* layoutName)
{
  return layoutName != nullptr && std::string(layoutName).rfind("SequenceTimeline", 0) == 0;
}

//------------------------------------------------------------------------------
void qMRMLLayoutSequenceLightboxViewFactory::applyTimelineConfiguration(vtkMRMLSequenceLightboxViewNode* viewNode)
{
  if (!viewNode)
  {
    return;
  }
  MRMLNodeModifyBlocker blocker(viewNode);
  viewNode->SetRows(1);
  viewNode->SetColumns(7);
  viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected);
  viewNode->SetHighlightSelectedItem(true);
}

//------------------------------------------------------------------------------
QWidget* qMRMLLayoutSequenceLightboxViewFactory::createViewFromNode(vtkMRMLAbstractViewNode* viewNode)
{
  if (!this->layoutManager() || !viewNode || !this->layoutManager()->viewport())
  {
    Q_ASSERT(viewNode);
    return nullptr;
  }
  vtkMRMLSequenceLightboxViewNode* lightboxViewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(viewNode);
  if (!lightboxViewNode)
  {
    return nullptr;
  }

  qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(this->mrmlScene(), lightboxViewNode);
  if (qMRMLLayoutSequenceLightboxViewFactory::isTimelineLayoutName(lightboxViewNode->GetLayoutName()) //
      && this->mrmlScene() && !this->mrmlScene()->IsImporting())
  {
    // The view node has just been created by the layout logic (view nodes that are loaded from a
    // scene file are added while the scene is importing, and they keep their settings).
    qMRMLLayoutSequenceLightboxViewFactory::applyTimelineConfiguration(lightboxViewNode);
  }

  qMRMLSequenceLightboxWidget* lightboxWidget = new qMRMLSequenceLightboxWidget(this->layoutManager()->viewport());
  lightboxWidget->setObjectName(QString("qMRMLSequenceLightboxWidget%1").arg(viewNode->GetLayoutName() ? viewNode->GetLayoutName() : ""));
  lightboxWidget->setMRMLScene(this->mrmlScene());
  lightboxWidget->setMRMLSequenceLightboxViewNode(lightboxViewNode);
  return lightboxWidget;
}
