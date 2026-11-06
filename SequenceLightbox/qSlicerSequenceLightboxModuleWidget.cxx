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

// Slicer includes
#include "qSlicerApplication.h"
#include "qSlicerLayoutManager.h"

// SequenceLightbox includes
#include "qSlicerSequenceLightboxModule.h"
#include "qSlicerSequenceLightboxModuleWidget.h"
#include "ui_qSlicerSequenceLightboxModuleWidget.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// Sequences includes
#include "vtkMRMLSequenceBrowserNode.h"

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkWeakPointer.h>

// STD includes
#include <vector>

//-----------------------------------------------------------------------------
class qSlicerSequenceLightboxModuleWidgetPrivate : public Ui_qSlicerSequenceLightboxModuleWidget
{
public:
  qSlicerSequenceLightboxModuleWidgetPrivate();

  /// Lightbox view node that the panel currently shows (observed for updates).
  vtkWeakPointer<vtkMRMLSequenceLightboxViewNode> ViewNode;
};

//-----------------------------------------------------------------------------
// qSlicerSequenceLightboxModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModuleWidgetPrivate::qSlicerSequenceLightboxModuleWidgetPrivate() = default;

//-----------------------------------------------------------------------------
// qSlicerSequenceLightboxModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModuleWidget::qSlicerSequenceLightboxModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerSequenceLightboxModuleWidgetPrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModuleWidget::~qSlicerSequenceLightboxModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::setup()
{
  Q_D(qSlicerSequenceLightboxModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();

  d->comboBox_LightboxLayout->addItem(tr("Lightbox next to the slice view"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxSliceView);
  d->comboBox_LightboxLayout->addItem(tr("Lightbox next to the 3D view"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightbox3DView);
  d->comboBox_LightboxLayout->addItem(tr("Timeline below the slice view"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimelineSliceView);
  d->comboBox_LightboxLayout->addItem(tr("Timeline below the 3D view"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimeline3DView);
  d->comboBox_LightboxLayout->addItem(tr("Lightbox only (slice)"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnlySliceView);
  d->comboBox_LightboxLayout->addItem(tr("Lightbox only (3D)"), qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnly3DView);

  connect(d->pushButton_ShowLightbox, SIGNAL(clicked()), this, SLOT(onShowLightboxButtonClicked()));
  connect(d->comboBox_LightboxLayout, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgetFromMRML()));
  // Only selections made by the user are applied (the selectors also change their current node
  // when their node list is rebuilt, for example while a scene is loaded).
  connect(d->MRMLNodeComboBox_SequenceBrowser, SIGNAL(nodeActivated(vtkMRMLNode*)), this, SLOT(onSequenceBrowserNodeChanged(vtkMRMLNode*)));
  connect(d->MRMLNodeComboBox_LightboxReferenceView, SIGNAL(nodeActivated(vtkMRMLNode*)), this, SLOT(onLightboxReferenceViewNodeChanged(vtkMRMLNode*)));
  connect(d->spinBox_LightboxRows, SIGNAL(valueChanged(int)), this, SLOT(onLightboxRowsChanged(int)));
  connect(d->spinBox_LightboxColumns, SIGNAL(valueChanged(int)), this, SLOT(onLightboxColumnsChanged(int)));

  // Switching layout can change which lightbox view the panel controls.
  if (qSlicerApplication::application() && qSlicerApplication::application()->layoutManager())
  {
    connect(qSlicerApplication::application()->layoutManager(), SIGNAL(layoutChanged(int)), this, SLOT(updateWidgetFromMRML()));
  }
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::enter()
{
  this->Superclass::enter();
  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  vtkMRMLScene* oldScene = this->mrmlScene();
  this->Superclass::setMRMLScene(scene);
  if (oldScene == scene)
  {
    return;
  }
  // Lightbox view nodes are created by the layout manager and are reset when the scene is
  // closed: the panel follows the node that is displayed.
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(updateWidgetFromMRML()));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(updateWidgetFromMRML()));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::EndImportEvent, this, SLOT(updateWidgetFromMRML()));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::EndCloseEvent, this, SLOT(updateWidgetFromMRML()));
  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode* qSlicerSequenceLightboxModuleWidget::lightboxViewNode()
{
  if (!this->mrmlScene())
  {
    return nullptr;
  }
  // There is a lightbox view node for each lightbox layout (a slice one and a 3D one). Control
  // the one that is currently displayed, so that the panel always acts on the view that the
  // user is looking at.
  std::vector<vtkMRMLNode*> viewNodes;
  this->mrmlScene()->GetNodesByClass("vtkMRMLSequenceLightboxViewNode", viewNodes);
  for (vtkMRMLNode* node : viewNodes)
  {
    vtkMRMLSequenceLightboxViewNode* viewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(node);
    if (viewNode && viewNode->IsMappedInLayout())
    {
      return viewNode;
    }
  }
  // No lightbox is displayed: the panel shows the settings of the lightbox of the layout that the
  // "Show" button switches to (if that view node exists already).
  const char* layoutName = qSlicerSequenceLightboxModule::SequenceLightboxSliceViewLayoutName;
  switch (this->selectedLightboxLayout())
  {
    case qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightbox3DView:
    case qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnly3DView: layoutName = qSlicerSequenceLightboxModule::SequenceLightbox3DViewLayoutName; break;
    case qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimelineSliceView: layoutName = qSlicerSequenceLightboxModule::SequenceTimelineSliceViewLayoutName; break;
    case qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimeline3DView: layoutName = qSlicerSequenceLightboxModule::SequenceTimeline3DViewLayoutName; break;
    default: break;
  }
  return vtkMRMLSequenceLightboxViewNode::SafeDownCast(this->mrmlScene()->GetSingletonNode(layoutName, "vtkMRMLSequenceLightboxViewNode"));
}

//-----------------------------------------------------------------------------
int qSlicerSequenceLightboxModuleWidget::selectedLightboxLayout()
{
  Q_D(qSlicerSequenceLightboxModuleWidget);
  QVariant layout = d->comboBox_LightboxLayout->currentData();
  return (layout.isValid() ? layout.toInt() : static_cast<int>(qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxSliceView));
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerSequenceLightboxModuleWidget);
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  if (viewNode != d->ViewNode)
  {
    this->qvtkReconnect(d->ViewNode, viewNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromMRML()));
    this->qvtkReconnect(d->ViewNode, viewNode, vtkMRMLSequenceLightboxViewNode::SequenceBrowserNodeModifiedEvent, this, SLOT(updateWidgetFromMRML()));
    this->qvtkReconnect(d->ViewNode, viewNode, vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, this, SLOT(updateWidgetFromMRML()));
    d->ViewNode = viewNode;
  }

  // The sequence browser can be chosen before the lightbox view exists (it is applied when the
  // layout is shown), the other settings belong to the view node.
  d->MRMLNodeComboBox_LightboxReferenceView->setEnabled(viewNode != nullptr);
  d->spinBox_LightboxRows->setEnabled(viewNode != nullptr);
  d->spinBox_LightboxColumns->setEnabled(viewNode != nullptr);
  if (!viewNode)
  {
    return;
  }

  bool wasBlocked = d->MRMLNodeComboBox_SequenceBrowser->blockSignals(true);
  d->MRMLNodeComboBox_SequenceBrowser->setCurrentNode(viewNode->GetSequenceBrowserNode());
  d->MRMLNodeComboBox_SequenceBrowser->blockSignals(wasBlocked);

  wasBlocked = d->MRMLNodeComboBox_LightboxReferenceView->blockSignals(true);
  d->MRMLNodeComboBox_LightboxReferenceView->setCurrentNode(viewNode->GetReferenceViewNode());
  d->MRMLNodeComboBox_LightboxReferenceView->blockSignals(wasBlocked);

  wasBlocked = d->spinBox_LightboxRows->blockSignals(true);
  d->spinBox_LightboxRows->setValue(viewNode->GetRows());
  d->spinBox_LightboxRows->blockSignals(wasBlocked);

  wasBlocked = d->spinBox_LightboxColumns->blockSignals(true);
  d->spinBox_LightboxColumns->setValue(viewNode->GetColumns());
  d->spinBox_LightboxColumns->blockSignals(wasBlocked);
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::onShowLightboxButtonClicked()
{
  Q_D(qSlicerSequenceLightboxModuleWidget);
  qSlicerApplication* app = qSlicerApplication::application();
  if (!app || !app->layoutManager())
  {
    return;
  }
  app->layoutManager()->setLayout(this->selectedLightboxLayout());

  // Show the browser node that the user has chosen (the view keeps its own browser node, or gets
  // the first one of the scene, if nothing is chosen).
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  vtkMRMLNode* browserNode = d->MRMLNodeComboBox_SequenceBrowser->currentNode();
  if (viewNode && browserNode)
  {
    viewNode->SetAndObserveSequenceBrowserNodeID(browserNode->GetID());
  }
  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::onSequenceBrowserNodeChanged(vtkMRMLNode* node)
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  if (!viewNode)
  {
    // Applied when the lightbox layout is shown.
    return;
  }
  viewNode->SetAndObserveSequenceBrowserNodeID(node ? node->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::onLightboxReferenceViewNodeChanged(vtkMRMLNode* node)
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  if (!viewNode)
  {
    return;
  }
  viewNode->SetAndObserveReferenceViewNodeID(node ? node->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::onLightboxRowsChanged(int rows)
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  if (!viewNode)
  {
    return;
  }
  viewNode->SetRows(rows);
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModuleWidget::onLightboxColumnsChanged(int columns)
{
  vtkMRMLSequenceLightboxViewNode* viewNode = this->lightboxViewNode();
  if (!viewNode)
  {
    return;
  }
  viewNode->SetColumns(columns);
}
