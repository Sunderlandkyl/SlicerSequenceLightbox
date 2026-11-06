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
#include <QVBoxLayout>

// Sequences includes
#include "qMRMLSequenceLightboxView.h"
#include "qMRMLSequenceLightboxViewControllerWidget.h"
#include "qMRMLSequenceLightboxWidget.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// MRMLDisplayableManager includes
#include <vtkMRMLAbstractDisplayableManager.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSequenceBrowserNode.h>

//--------------------------------------------------------------------------
class qMRMLSequenceLightboxWidgetPrivate
{
  Q_DECLARE_PUBLIC(qMRMLSequenceLightboxWidget);

protected:
  qMRMLSequenceLightboxWidget* const q_ptr;

public:
  qMRMLSequenceLightboxWidgetPrivate(qMRMLSequenceLightboxWidget& object);

  void init();

  qMRMLSequenceLightboxView* LightboxView{ nullptr };
  qMRMLSequenceLightboxViewControllerWidget* LightboxController{ nullptr };
};

//---------------------------------------------------------------------------
qMRMLSequenceLightboxWidgetPrivate::qMRMLSequenceLightboxWidgetPrivate(qMRMLSequenceLightboxWidget& object)
  : q_ptr(&object)
{
}

//---------------------------------------------------------------------------
void qMRMLSequenceLightboxWidgetPrivate::init()
{
  Q_Q(qMRMLSequenceLightboxWidget);

  QVBoxLayout* layout = new QVBoxLayout(q);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);

  this->LightboxController = new qMRMLSequenceLightboxViewControllerWidget;
  layout->addWidget(this->LightboxController);

  this->LightboxView = new qMRMLSequenceLightboxView;
  this->LightboxView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  layout->addWidget(this->LightboxView);

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), this->LightboxView, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), this->LightboxController, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->LightboxController, SIGNAL(showAllItemsRequested()), q, SLOT(showAllItems()));
}

// --------------------------------------------------------------------------
// qMRMLSequenceLightboxWidget methods

// --------------------------------------------------------------------------
qMRMLSequenceLightboxWidget::qMRMLSequenceLightboxWidget(QWidget* parentWidget)
  : Superclass(parentWidget)
  , d_ptr(new qMRMLSequenceLightboxWidgetPrivate(*this))
{
  Q_D(qMRMLSequenceLightboxWidget);
  d->init();
}

// --------------------------------------------------------------------------
qMRMLSequenceLightboxWidget::~qMRMLSequenceLightboxWidget()
{
  Q_D(qMRMLSequenceLightboxWidget);
  d->LightboxView->setMRMLScene(nullptr);
  d->LightboxController->setMRMLScene(nullptr);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxWidget::setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* newViewNode)
{
  Q_D(qMRMLSequenceLightboxWidget);
  d->LightboxView->setMRMLSequenceLightboxViewNode(newViewNode);
  d->LightboxController->setMRMLSequenceLightboxViewNode(newViewNode);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxWidget::setMRMLAbstractViewNode(vtkMRMLAbstractViewNode* newViewNode)
{
  vtkMRMLSequenceLightboxViewNode* lightboxViewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(newViewNode);
  if (newViewNode && !lightboxViewNode)
  {
    qWarning() << Q_FUNC_INFO << " failed: Invalid view node type " << newViewNode->GetClassName() << ". Expected node type: vtkMRMLSequenceLightboxViewNode";
  }
  this->setMRMLSequenceLightboxViewNode(lightboxViewNode);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxWidget::showAllItems()
{
  Q_D(qMRMLSequenceLightboxWidget);
  vtkMRMLSequenceLightboxViewNode* viewNode = this->mrmlSequenceLightboxViewNode();
  vtkMRMLSequenceBrowserNode* browserNode = (viewNode ? viewNode->GetSequenceBrowserNode() : nullptr);
  if (!browserNode)
  {
    return;
  }
  int numberOfItems = browserNode->GetNumberOfItems();
  if (viewNode->GetItemSelectionMode() == vtkMRMLSequenceLightboxViewNode::ItemSelectionCustom)
  {
    // All the items of the custom list.
    numberOfItems = static_cast<int>(viewNode->GetCustomItemNumbers().size());
  }
  else
  {
    // Consecutive items from the first one.
    viewNode->SetItemSelectionMode(vtkMRMLSequenceLightboxViewNode::ItemSelectionFixed);
    viewNode->SetStartItemNumber(0);
    numberOfItems = (numberOfItems + viewNode->GetItemStep() - 1) / viewNode->GetItemStep();
  }
  viewNode->FitGridToItems(numberOfItems, d->LightboxView->width(), d->LightboxView->height());
}

// --------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode* qMRMLSequenceLightboxWidget::mrmlSequenceLightboxViewNode() const
{
  Q_D(const qMRMLSequenceLightboxWidget);
  return d->LightboxView->mrmlSequenceLightboxViewNode();
}

// --------------------------------------------------------------------------
vtkMRMLAbstractViewNode* qMRMLSequenceLightboxWidget::mrmlAbstractViewNode() const
{
  return this->mrmlSequenceLightboxViewNode();
}

// --------------------------------------------------------------------------
qMRMLSequenceLightboxView* qMRMLSequenceLightboxWidget::sequenceLightboxView() const
{
  Q_D(const qMRMLSequenceLightboxWidget);
  return d->LightboxView;
}

// --------------------------------------------------------------------------
QWidget* qMRMLSequenceLightboxWidget::viewWidget() const
{
  return this->sequenceLightboxView();
}

// --------------------------------------------------------------------------
qMRMLSequenceLightboxViewControllerWidget* qMRMLSequenceLightboxWidget::sequenceLightboxController() const
{
  Q_D(const qMRMLSequenceLightboxWidget);
  return d->LightboxController;
}

// --------------------------------------------------------------------------
qMRMLViewControllerBar* qMRMLSequenceLightboxWidget::controllerWidget() const
{
  return this->sequenceLightboxController();
}

// --------------------------------------------------------------------------
vtkMRMLAbstractDisplayableManager* qMRMLSequenceLightboxWidget::displayableManagerByClassName(const char* className)
{
  Q_D(qMRMLSequenceLightboxWidget);
  return d->LightboxView->displayableManagerByClassName(className);
}
