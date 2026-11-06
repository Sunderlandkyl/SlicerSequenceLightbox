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
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

// CTK includes
#include <ctkPopupWidget.h>

// Sequences includes
#include "qMRMLSequenceLightboxViewControllerWidget.h"
#include "qMRMLSequenceLightboxViewControllerWidget_p.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// qMRML includes
#include <qMRMLNodeComboBox.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSequenceBrowserNode.h>

// VTK includes
#include <vtkWeakPointer.h>

//-----------------------------------------------------------------------------
qMRMLSequenceLightboxViewControllerWidgetPrivate::qMRMLSequenceLightboxViewControllerWidgetPrivate(qMRMLSequenceLightboxViewControllerWidget& object)
  : Superclass(object)
  , q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qMRMLSequenceLightboxViewControllerWidgetPrivate::~qMRMLSequenceLightboxViewControllerWidgetPrivate() = default;

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidgetPrivate::setupPopupUi()
{
  // The popup is only shown when the controller bar is pinned or hovered, so it holds the
  // settings that are changed less often.
  this->Superclass::setupPopupUi();

  QFormLayout* popupLayout = new QFormLayout(this->PopupWidget);

  this->ItemSelectionModeComboBox = new QComboBox(this->PopupWidget);
  this->ItemSelectionModeComboBox->setObjectName("ItemSelectionModeComboBox");
  this->ItemSelectionModeComboBox->addItem(qMRMLSequenceLightboxViewControllerWidget::tr("Consecutive items from a fixed item"),
                                           vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed);
  this->ItemSelectionModeComboBox->addItem(qMRMLSequenceLightboxViewControllerWidget::tr("Consecutive items from the selected item"),
                                           vtkMRMLSequenceLightboxViewNode::ItemSelectionFollowSelected);
  this->ItemSelectionModeComboBox->addItem(qMRMLSequenceLightboxViewControllerWidget::tr("Consecutive items around the selected item"),
                                           vtkMRMLSequenceLightboxViewNode::ItemSelectionCenterSelected);
  this->ItemSelectionModeComboBox->addItem(qMRMLSequenceLightboxViewControllerWidget::tr("Custom list of items"), vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom);
  this->ItemSelectionModeComboBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Determines which items are displayed in the tiles."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Displayed items:"), this->ItemSelectionModeComboBox);

  this->ItemStepSpinBox = new QSpinBox(this->PopupWidget);
  this->ItemStepSpinBox->setObjectName("ItemStepSpinBox");
  this->ItemStepSpinBox->setRange(1, 1000);
  this->ItemStepSpinBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Difference between the item numbers displayed in consecutive tiles."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Item step:"), this->ItemStepSpinBox);

  this->StartItemNumberSpinBox = new QSpinBox(this->PopupWidget);
  this->StartItemNumberSpinBox->setObjectName("StartItemNumberSpinBox");
  this->StartItemNumberSpinBox->setRange(0, 100000);
  this->StartItemNumberSpinBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Item displayed in the first tile."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("First item:"), this->StartItemNumberSpinBox);

  this->CustomItemNumbersLineEdit = new QLineEdit(this->PopupWidget);
  this->CustomItemNumbersLineEdit->setObjectName("CustomItemNumbersLineEdit");
  this->CustomItemNumbersLineEdit->setPlaceholderText(qMRMLSequenceLightboxViewControllerWidget::tr("for example: 0, 3, 7-9"));
  this->CustomItemNumbersLineEdit->setToolTip(
    qMRMLSequenceLightboxViewControllerWidget::tr("Items to display, in the order of the tiles. Ranges can be given as 7-9. Item numbers start at 0."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Items:"), this->CustomItemNumbersLineEdit);

  this->ShowAllItemsButton = new QPushButton(qMRMLSequenceLightboxViewControllerWidget::tr("Show all items"), this->PopupWidget);
  this->ShowAllItemsButton->setObjectName("ShowAllItemsButton");
  this->ShowAllItemsButton->setToolTip(
    qMRMLSequenceLightboxViewControllerWidget::tr("Choose the number of rows and columns so that every item of the sequence is displayed, with tiles as large as possible."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Grid size:"), this->ShowAllItemsButton);

  this->TileSpacingSpinBox = new QSpinBox(this->PopupWidget);
  this->TileSpacingSpinBox->setObjectName("TileSpacingSpinBox");
  this->TileSpacingSpinBox->setRange(0, 50);
  this->TileSpacingSpinBox->setSuffix(qMRMLSequenceLightboxViewControllerWidget::tr(" px"));
  this->TileSpacingSpinBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Width of the gap between the tiles."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Tile spacing:"), this->TileSpacingSpinBox);

  this->ShowItemLabelsCheckBox = new QCheckBox(this->PopupWidget);
  this->ShowItemLabelsCheckBox->setObjectName("ShowItemLabelsCheckBox");
  this->ShowItemLabelsCheckBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Show the index value of the displayed item in each tile."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Show item labels:"), this->ShowItemLabelsCheckBox);

  this->HighlightSelectedItemCheckBox = new QCheckBox(this->PopupWidget);
  this->HighlightSelectedItemCheckBox->setObjectName("HighlightSelectedItemCheckBox");
  this->HighlightSelectedItemCheckBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Draw a border around the tile that shows the currently selected item."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Highlight selected item:"), this->HighlightSelectedItemCheckBox);

  this->InteractiveTilesCheckBox = new QCheckBox(this->PopupWidget);
  this->InteractiveTilesCheckBox->setObjectName("InteractiveTilesCheckBox");
  this->InteractiveTilesCheckBox->setToolTip(
    qMRMLSequenceLightboxViewControllerWidget::tr("Allow interacting with the tiles like with regular views: scroll, pan and zoom the slice, rotate the 3D camera, "
                                                  "place and move markups, etc. Changes of the view are applied to the reference view, changes of a sequence "
                                                  "item are stored in that item. If disabled then clicking a tile only selects its item."));
  popupLayout->addRow(qMRMLSequenceLightboxViewControllerWidget::tr("Interactive tiles:"), this->InteractiveTilesCheckBox);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidgetPrivate::init()
{
  Q_Q(qMRMLSequenceLightboxViewControllerWidget);

  // Creates the bar and the popup (which is filled by setupPopupUi).
  this->Superclass::init();

  this->ViewLabel->setText(qMRMLSequenceLightboxViewControllerWidget::tr("L"));
  this->BarLayout->addStretch(1);

  // Bar: the settings that are changed most often.
  this->SequenceBrowserNodeSelector = new qMRMLNodeComboBox(q);
  this->SequenceBrowserNodeSelector->setObjectName("SequenceBrowserNodeSelector");
  this->SequenceBrowserNodeSelector->setNodeTypes(QStringList() << "vtkMRMLSequenceBrowserNode");
  this->SequenceBrowserNodeSelector->setAddEnabled(false);
  this->SequenceBrowserNodeSelector->setRemoveEnabled(false);
  this->SequenceBrowserNodeSelector->setRenameEnabled(false);
  this->SequenceBrowserNodeSelector->setNoneEnabled(true);
  this->SequenceBrowserNodeSelector->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Sequence browser that is displayed in the tiles."));
  this->BarLayout->addWidget(this->SequenceBrowserNodeSelector);

  this->ReferenceViewNodeSelector = new qMRMLNodeComboBox(q);
  this->ReferenceViewNodeSelector->setObjectName("ReferenceViewNodeSelector");
  this->ReferenceViewNodeSelector->setNodeTypes(QStringList() << "vtkMRMLSliceNode"
                                                              << "vtkMRMLViewNode");
  this->ReferenceViewNodeSelector->setAddEnabled(false);
  this->ReferenceViewNodeSelector->setRemoveEnabled(false);
  this->ReferenceViewNodeSelector->setRenameEnabled(false);
  this->ReferenceViewNodeSelector->setNoneEnabled(true);
  // View nodes are hidden from editors, so they are only listed if hidden nodes are shown.
  this->ReferenceViewNodeSelector->setShowHidden(true);
  this->ReferenceViewNodeSelector->setToolTip(
    qMRMLSequenceLightboxViewControllerWidget::tr("View that the tiles follow. Slice position, zoom, window/level and camera are taken from this view."));
  this->BarLayout->addWidget(this->ReferenceViewNodeSelector);

  this->RowsSpinBox = new QSpinBox(q);
  this->RowsSpinBox->setObjectName("RowsSpinBox");
  this->RowsSpinBox->setRange(1, 16);
  this->RowsSpinBox->setPrefix(qMRMLSequenceLightboxViewControllerWidget::tr("Rows: "));
  this->RowsSpinBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Number of tile rows."));
  this->BarLayout->addWidget(this->RowsSpinBox);

  this->ColumnsSpinBox = new QSpinBox(q);
  this->ColumnsSpinBox->setObjectName("ColumnsSpinBox");
  this->ColumnsSpinBox->setRange(1, 16);
  this->ColumnsSpinBox->setPrefix(qMRMLSequenceLightboxViewControllerWidget::tr("Columns: "));
  this->ColumnsSpinBox->setToolTip(qMRMLSequenceLightboxViewControllerWidget::tr("Number of tile columns."));
  this->BarLayout->addWidget(this->ColumnsSpinBox);

  // Only selections made by the user are applied to the view node: the node selectors also
  // change their current node when the node list is rebuilt (for example while a scene is
  // loaded), which must not overwrite the references of the view node.
  QObject::connect(this->SequenceBrowserNodeSelector, SIGNAL(nodeActivated(vtkMRMLNode*)), q, SLOT(onSequenceBrowserNodeChanged(vtkMRMLNode*)));
  QObject::connect(this->ReferenceViewNodeSelector, SIGNAL(nodeActivated(vtkMRMLNode*)), q, SLOT(onReferenceViewNodeChanged(vtkMRMLNode*)));
  QObject::connect(this->RowsSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onRowsChanged(int)));
  QObject::connect(this->ColumnsSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onColumnsChanged(int)));
  QObject::connect(this->TileSpacingSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onTileSpacingChanged(int)));
  QObject::connect(this->ShowAllItemsButton, SIGNAL(clicked()), q, SIGNAL(showAllItemsRequested()));
  QObject::connect(this->ItemStepSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onItemStepChanged(int)));
  QObject::connect(this->ItemSelectionModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onItemSelectionModeChanged(int)));
  QObject::connect(this->StartItemNumberSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onStartItemNumberChanged(int)));
  QObject::connect(this->CustomItemNumbersLineEdit, SIGNAL(editingFinished()), q, SLOT(onCustomItemNumbersChanged()));
  QObject::connect(this->ShowItemLabelsCheckBox, SIGNAL(toggled(bool)), q, SLOT(onShowItemLabelsToggled(bool)));
  QObject::connect(this->HighlightSelectedItemCheckBox, SIGNAL(toggled(bool)), q, SLOT(onHighlightSelectedItemToggled(bool)));
  QObject::connect(this->InteractiveTilesCheckBox, SIGNAL(toggled(bool)), q, SLOT(onInteractiveTilesToggled(bool)));
}

//-----------------------------------------------------------------------------
qMRMLSequenceLightboxViewControllerWidget::qMRMLSequenceLightboxViewControllerWidget(QWidget* parentWidget)
  : Superclass(new qMRMLSequenceLightboxViewControllerWidgetPrivate(*this), parentWidget)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qMRMLSequenceLightboxViewControllerWidget::~qMRMLSequenceLightboxViewControllerWidget() = default;

//-----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode* qMRMLSequenceLightboxViewControllerWidget::mrmlSequenceLightboxViewNode() const
{
  Q_D(const qMRMLSequenceLightboxViewControllerWidget);
  return d->LightboxViewNode;
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::setMRMLScene(vtkMRMLScene* newScene)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (this->mrmlScene() == newScene)
  {
    return;
  }
  this->Superclass::setMRMLScene(newScene);
  d->SequenceBrowserNodeSelector->setMRMLScene(newScene);
  d->ReferenceViewNodeSelector->setMRMLScene(newScene);
  this->updateWidgetFromMRMLView();
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* viewNode)
{
  this->setMRMLViewNode(viewNode);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::setMRMLViewNode(vtkMRMLAbstractViewNode* viewNode)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  vtkMRMLSequenceLightboxViewNode* lightboxViewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(viewNode);
  if (viewNode && !lightboxViewNode)
  {
    qWarning() << Q_FUNC_INFO << " failed: Invalid view node type " << viewNode->GetClassName();
  }
  d->LightboxViewNode = lightboxViewNode;
  this->Superclass::setMRMLViewNode(lightboxViewNode);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::setViewLabel(const QString& newViewLabel)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode)
  {
    qWarning() << Q_FUNC_INFO << " failed: view node is not set";
    return;
  }
  d->LightboxViewNode->SetLayoutLabel(newViewLabel.toUtf8());
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::updateWidgetFromMRMLView()
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  this->Superclass::updateWidgetFromMRMLView();

  bool enabled = (d->LightboxViewNode != nullptr);
  d->SequenceBrowserNodeSelector->setEnabled(enabled);
  d->ReferenceViewNodeSelector->setEnabled(enabled);
  d->RowsSpinBox->setEnabled(enabled);
  d->ColumnsSpinBox->setEnabled(enabled);
  d->TileSpacingSpinBox->setEnabled(enabled);
  d->ShowAllItemsButton->setEnabled(enabled);
  d->ItemStepSpinBox->setEnabled(enabled);
  d->ItemSelectionModeComboBox->setEnabled(enabled);
  d->ShowItemLabelsCheckBox->setEnabled(enabled);
  d->HighlightSelectedItemCheckBox->setEnabled(enabled);
  d->InteractiveTilesCheckBox->setEnabled(enabled);
  if (!enabled)
  {
    d->StartItemNumberSpinBox->setEnabled(false);
    d->CustomItemNumbersLineEdit->setEnabled(false);
    return;
  }

  bool wasUpdating = d->UpdatingWidgetFromMRML;
  d->UpdatingWidgetFromMRML = true;

  // The label is defined by the layout ("L" for a lightbox, "T" for a timeline).
  if (d->LightboxViewNode->GetLayoutLabel() && d->LightboxViewNode->GetLayoutLabel()[0] != '\0')
  {
    d->ViewLabel->setText(QString::fromUtf8(d->LightboxViewNode->GetLayoutLabel()));
  }
  d->SequenceBrowserNodeSelector->setCurrentNode(d->LightboxViewNode->GetSequenceBrowserNode());
  d->ReferenceViewNodeSelector->setCurrentNode(d->LightboxViewNode->GetReferenceViewNode());
  d->RowsSpinBox->setValue(d->LightboxViewNode->GetRows());
  d->ColumnsSpinBox->setValue(d->LightboxViewNode->GetColumns());
  d->TileSpacingSpinBox->setValue(d->LightboxViewNode->GetTileSpacing());
  int itemSelectionMode = d->LightboxViewNode->GetItemSelectionMode();
  bool customItems = (itemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom);

  d->ItemStepSpinBox->setValue(d->LightboxViewNode->GetItemStep());
  d->ItemStepSpinBox->setEnabled(!customItems);
  d->ItemSelectionModeComboBox->setCurrentIndex(d->ItemSelectionModeComboBox->findData(itemSelectionMode));
  d->StartItemNumberSpinBox->setValue(d->LightboxViewNode->GetStartItemNumber());
  d->StartItemNumberSpinBox->setEnabled(itemSelectionMode == vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed);
  d->CustomItemNumbersLineEdit->setText(QString::fromStdString(d->LightboxViewNode->GetCustomItemNumbersAsString()));
  d->CustomItemNumbersLineEdit->setEnabled(customItems);
  d->ShowItemLabelsCheckBox->setChecked(d->LightboxViewNode->GetShowItemLabels());
  d->HighlightSelectedItemCheckBox->setChecked(d->LightboxViewNode->GetHighlightSelectedItem());
  d->InteractiveTilesCheckBox->setChecked(d->LightboxViewNode->GetInteractiveTiles());

  d->UpdatingWidgetFromMRML = wasUpdating;
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onSequenceBrowserNodeChanged(vtkMRMLNode* node)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetAndObserveSequenceBrowserNodeID(node ? node->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onReferenceViewNodeChanged(vtkMRMLNode* node)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetAndObserveReferenceViewNodeID(node ? node->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onRowsChanged(int rows)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetRows(rows);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onColumnsChanged(int columns)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetColumns(columns);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onTileSpacingChanged(int tileSpacing)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetTileSpacing(tileSpacing);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onItemStepChanged(int itemStep)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetItemStep(itemStep);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onItemSelectionModeChanged(int index)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetItemSelectionMode(d->ItemSelectionModeComboBox->itemData(index).toInt());
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onCustomItemNumbersChanged()
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetCustomItemNumbersFromString(d->CustomItemNumbersLineEdit->text().toStdString());
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onStartItemNumberChanged(int startItemNumber)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetStartItemNumber(startItemNumber);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onShowItemLabelsToggled(bool show)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetShowItemLabels(show);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onHighlightSelectedItemToggled(bool highlight)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetHighlightSelectedItem(highlight);
}

//-----------------------------------------------------------------------------
void qMRMLSequenceLightboxViewControllerWidget::onInteractiveTilesToggled(bool interactive)
{
  Q_D(qMRMLSequenceLightboxViewControllerWidget);
  if (!d->LightboxViewNode || d->UpdatingWidgetFromMRML)
  {
    return;
  }
  d->LightboxViewNode->SetInteractiveTiles(interactive);
}
