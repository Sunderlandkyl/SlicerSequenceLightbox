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

// Sequences includes
#include "qMRMLSequenceLightboxView.h"
#include "vtkMRMLSequenceLightboxDisplayableManager.h"
#include "vtkMRMLSequenceLightboxViewDisplayableManagerFactory.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// MRMLDisplayableManager includes
#include <vtkMRMLAbstractDisplayableManager.h>
#include <vtkMRMLDisplayableManagerGroup.h>
#include <vtkMRMLViewInteractorStyle.h>

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkInteractorStyleUser.h>
#include <vtkNew.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

//--------------------------------------------------------------------------
class qMRMLSequenceLightboxViewPrivate
{
public:
  qMRMLSequenceLightboxViewPrivate() = default;

  vtkSmartPointer<vtkMRMLDisplayableManagerGroup> DisplayableManagerGroup;
  vtkSmartPointer<vtkMRMLViewInteractorStyle> InteractorObserver;
  vtkWeakPointer<vtkMRMLScene> MRMLScene;
  vtkWeakPointer<vtkMRMLSequenceLightboxViewNode> MRMLViewNode;
};

// --------------------------------------------------------------------------
qMRMLSequenceLightboxView::qMRMLSequenceLightboxView(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qMRMLSequenceLightboxViewPrivate)
{
  Q_D(qMRMLSequenceLightboxView);

  this->setRenderEnabled(false);

  // The lightbox view itself is not navigated: it is a grid of independent views that follow
  // a reference view. An interactor style that does nothing by itself makes sure that the
  // camera of the view is never manipulated.
  if (this->interactor())
  {
    vtkNew<vtkInteractorStyleUser> interactorStyle;
    this->interactor()->SetInteractorStyle(interactorStyle);
  }

  // The orientation marker of ctkVTKRenderView would add a renderer of its own.
  this->setOrientationWidgetVisible(false);

  d->InteractorObserver = vtkSmartPointer<vtkMRMLViewInteractorStyle>::New();

  vtkMRMLSequenceLightboxViewDisplayableManagerFactory* factory = vtkMRMLSequenceLightboxViewDisplayableManagerFactory::GetInstance();
  if (!factory->IsDisplayableManagerRegistered("vtkMRMLSequenceLightboxDisplayableManager"))
  {
    factory->RegisterDisplayableManager("vtkMRMLSequenceLightboxDisplayableManager");
  }
  d->DisplayableManagerGroup = vtkSmartPointer<vtkMRMLDisplayableManagerGroup>::Take(factory->InstantiateDisplayableManagers(this->renderer()));
  d->InteractorObserver->SetDisplayableManagers(d->DisplayableManagerGroup);
  if (this->interactor())
  {
    d->InteractorObserver->SetInteractor(this->interactor());
  }
  // Observe the displayable manager group to catch RequestRender events.
  this->qvtkConnect(d->DisplayableManagerGroup, vtkCommand::UpdateEvent, this, SLOT(scheduleRender()));
}

// --------------------------------------------------------------------------
qMRMLSequenceLightboxView::~qMRMLSequenceLightboxView()
{
  Q_D(qMRMLSequenceLightboxView);
  if (d->DisplayableManagerGroup)
  {
    d->DisplayableManagerGroup->SetMRMLDisplayableNode(nullptr);
  }
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::setInteractor(vtkRenderWindowInteractor* interactor)
{
  Q_D(qMRMLSequenceLightboxView);
  this->Superclass::setInteractor(interactor);
  if (interactor)
  {
    vtkNew<vtkInteractorStyleUser> interactorStyle;
    interactor->SetInteractorStyle(interactorStyle);
  }
  d->InteractorObserver->SetInteractor(interactor);
}

// --------------------------------------------------------------------------
vtkMRMLViewInteractorStyle* qMRMLSequenceLightboxView::interactorObserver() const
{
  Q_D(const qMRMLSequenceLightboxView);
  return d->InteractorObserver;
}

// --------------------------------------------------------------------------
vtkMRMLDisplayableManagerGroup* qMRMLSequenceLightboxView::displayableManagerGroup() const
{
  Q_D(const qMRMLSequenceLightboxView);
  return d->DisplayableManagerGroup;
}

// --------------------------------------------------------------------------
vtkMRMLAbstractDisplayableManager* qMRMLSequenceLightboxView::displayableManagerByClassName(const char* className)
{
  Q_D(qMRMLSequenceLightboxView);
  if (!d->DisplayableManagerGroup)
  {
    return nullptr;
  }
  return d->DisplayableManagerGroup->GetDisplayableManagerByClassName(className);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::forceRender()
{
  // The tiles collect the changes of the scene and apply them together, right before the view is
  // rendered. This has to be done outside of the render (some displayable managers ignore the
  // modifications made while rendering is in progress), so the view does it here.
  vtkMRMLSequenceLightboxDisplayableManager* lightboxDisplayableManager =
    vtkMRMLSequenceLightboxDisplayableManager::SafeDownCast(this->displayableManagerByClassName("vtkMRMLSequenceLightboxDisplayableManager"));
  if (lightboxDisplayableManager)
  {
    lightboxDisplayableManager->SynchronizeTiles();
  }
  this->Superclass::forceRender();
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::setMRMLScene(vtkMRMLScene* newScene)
{
  Q_D(qMRMLSequenceLightboxView);
  if (d->MRMLScene == newScene)
  {
    return;
  }
  this->qvtkReconnect(d->MRMLScene, newScene, vtkMRMLScene::StartBatchProcessEvent, this, SLOT(onSceneStartProcessing()));
  this->qvtkReconnect(d->MRMLScene, newScene, vtkMRMLScene::EndBatchProcessEvent, this, SLOT(onSceneEndProcessing()));
  d->MRMLScene = newScene;
  this->setRenderEnabled(newScene != nullptr && !newScene->IsBatchProcessing());
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* newViewNode)
{
  Q_D(qMRMLSequenceLightboxView);
  if (d->MRMLViewNode == newViewNode)
  {
    return;
  }
  this->qvtkReconnect(d->MRMLViewNode, newViewNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  d->MRMLViewNode = newViewNode;
  if (d->DisplayableManagerGroup)
  {
    d->DisplayableManagerGroup->SetMRMLDisplayableNode(newViewNode);
  }
  this->updateWidgetFromMRML();
  this->setEnabled(newViewNode != nullptr);
}

// --------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewNode* qMRMLSequenceLightboxView::mrmlSequenceLightboxViewNode() const
{
  Q_D(const qMRMLSequenceLightboxView);
  return d->MRMLViewNode;
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::updateWidgetFromMRML()
{
  Q_D(qMRMLSequenceLightboxView);
  if (!d->MRMLViewNode)
  {
    return;
  }
  // The background is only set on the renderer of the lightbox view itself. The tiles have
  // renderers of their own in the same render window, whose background is set by the displayable
  // managers of the tile from the mirrored reference view node (for example the gradient
  // background of a 3D view); the base class setters would overwrite it in every renderer.
  vtkRenderer* renderer = this->renderer();
  if (!renderer)
  {
    return;
  }
  double backgroundColor[3] = { 0.0, 0.0, 0.0 };
  d->MRMLViewNode->GetBackgroundColor(backgroundColor);
  renderer->SetGradientBackground(false);
  renderer->SetBackground(backgroundColor);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::onSceneStartProcessing()
{
  this->setRenderEnabled(false);
}

// --------------------------------------------------------------------------
void qMRMLSequenceLightboxView::onSceneEndProcessing()
{
  this->setRenderEnabled(true);
}
