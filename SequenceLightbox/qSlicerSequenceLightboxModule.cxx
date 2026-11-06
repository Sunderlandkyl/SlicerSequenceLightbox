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
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QTimer>

// Slicer includes
#include "qSlicerApplication.h"
#include "qSlicerLayoutManager.h"

// SequenceLightbox includes
#include "qMRMLLayoutSequenceLightboxViewFactory.h"
#include "qSlicerSequenceLightboxModule.h"
#include "qSlicerSequenceLightboxModuleWidget.h"
#include "vtkMRMLSequenceLightboxViewDisplayableManagerFactory.h"
#include "vtkMRMLSequenceLightboxViewNode.h"
#include "vtkSlicerSequenceLightboxLogic.h"

// Sequences includes
#include "vtkMRMLSequenceBrowserNode.h"

// MRML includes
#include "vtkMRMLAbstractViewNode.h"
#include "vtkMRMLLayoutLogic.h"
#include "vtkMRMLLayoutNode.h"
#include "vtkMRMLScene.h"

// STD includes
#include <cstring>
#include <string>
#include <vector>

const char* qSlicerSequenceLightboxModule::SequenceLightboxSliceViewLayoutName = "SequenceLightbox1";
const char* qSlicerSequenceLightboxModule::SequenceLightbox3DViewLayoutName = "SequenceLightbox3D";
const char* qSlicerSequenceLightboxModule::SequenceTimelineSliceViewLayoutName = "SequenceTimeline1";
const char* qSlicerSequenceLightboxModule::SequenceTimeline3DViewLayoutName = "SequenceTimeline3D";

namespace
{

//-----------------------------------------------------------------------------
/// XML description of a view of a layout.
std::string viewLayoutDescription(const char* className, const char* singletonTag, const char* viewLabel, const char* extraProperties = "")
{
  return std::string("<view class=\"") + className + "\" singletontag=\"" + singletonTag + "\">" //
         + "<property name=\"viewlabel\" action=\"default\">" + viewLabel + "</property>"        //
         + extraProperties + "</view>";
}

//-----------------------------------------------------------------------------
/// XML description of a layout that splits the area between two views.
std::string splitLayoutDescription(const char* type, const std::string& firstView, int firstSize, const std::string& secondView, int secondSize)
{
  return std::string("<layout type=\"") + type + "\" split=\"true\">"                         //
         + "<item splitSize=\"" + std::to_string(firstSize) + "\">" + firstView + "</item>"   //
         + "<item splitSize=\"" + std::to_string(secondSize) + "\">" + secondView + "</item>" //
         + "</layout>";
}

//-----------------------------------------------------------------------------
/// XML description of a layout that contains a single view.
std::string singleViewLayoutDescription(const std::string& view)
{
  return std::string("<layout type=\"horizontal\"><item>") + view + "</item></layout>";
}

//-----------------------------------------------------------------------------
/// XML description of the view that is displayed next to a lightbox view: the reference view
/// that the tiles of the lightbox follow, so that the user sees the view that is tiled and can
/// navigate it. The default view (the view with the given class and layout name) is used until
/// the lightbox view node exists in the scene and has another reference view.
std::string companionViewLayoutDescription(vtkMRMLScene* scene,
                                           const char* lightboxLayoutName,
                                           const char* defaultViewClassName,
                                           const char* defaultViewLayoutName,
                                           const std::string& defaultView)
{
  vtkMRMLSequenceLightboxViewNode* lightboxViewNode =
    (scene ? vtkMRMLSequenceLightboxViewNode::SafeDownCast(scene->GetSingletonNode(lightboxLayoutName, "vtkMRMLSequenceLightboxViewNode")) : nullptr);
  vtkMRMLAbstractViewNode* referenceViewNode = (lightboxViewNode ? lightboxViewNode->GetReferenceViewNode() : nullptr);
  if (!referenceViewNode || !referenceViewNode->GetLayoutName() || !*referenceViewNode->GetLayoutName() //
      || referenceViewNode->IsA("vtkMRMLSequenceLightboxViewNode")                                      //
      || (std::strcmp(referenceViewNode->GetClassName(), defaultViewClassName) == 0                     //
          && std::strcmp(referenceViewNode->GetLayoutName(), defaultViewLayoutName) == 0))
  {
    return defaultView;
  }
  const char* viewLabel = referenceViewNode->GetLayoutLabel();
  return viewLayoutDescription(referenceViewNode->GetClassName(), referenceViewNode->GetLayoutName(), (viewLabel && *viewLabel) ? viewLabel : referenceViewNode->GetLayoutName());
}

} // namespace

//-----------------------------------------------------------------------------
class qSlicerSequenceLightboxModulePrivate
{
public:
  qSlicerSequenceLightboxModulePrivate();
  virtual ~qSlicerSequenceLightboxModulePrivate();

  /// Makes the sequence lightbox view available in the application: registers the view node
  /// factory in the layout manager and the displayable manager of the lightbox view.
  virtual void registerLightboxView();

  /// Adds the layouts that contain a sequence lightbox view to the layout node of the scene, or
  /// updates them if they are registered already: the view that is displayed next to a lightbox
  /// is the reference view of the lightbox view node.
  virtual void updateLightboxLayoutDescriptions();

  /// Adds the sequence lightbox layouts to the layout menu of the application.
  virtual void addLightboxLayoutMenuActions();

  /// Coalesces the lightbox layout updates and defers them to the event loop.
  QTimer UpdateLightboxLayoutDescriptionsTimer;
  bool LightboxLayoutMenuActionsAdded{ false };
};

//-----------------------------------------------------------------------------
// qSlicerSequenceLightboxModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModulePrivate::qSlicerSequenceLightboxModulePrivate() = default;

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModulePrivate::~qSlicerSequenceLightboxModulePrivate() = default;

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModulePrivate::registerLightboxView()
{
  qSlicerApplication* app = qSlicerApplication::application();
  if (!app)
  {
    return;
  }

  // The lightbox view has a displayable manager of its own, which creates a complete view
  // (renderer, scene, displayable managers) for each tile.
  vtkMRMLSequenceLightboxViewDisplayableManagerFactory* factory = vtkMRMLSequenceLightboxViewDisplayableManagerFactory::GetInstance();
  factory->SetMRMLApplicationLogic(app->applicationLogic());
  if (!factory->IsDisplayableManagerRegistered("vtkMRMLSequenceLightboxDisplayableManager"))
  {
    factory->RegisterDisplayableManager("vtkMRMLSequenceLightboxDisplayableManager");
  }

  qSlicerLayoutManager* layoutManager = app->layoutManager();
  if (!layoutManager)
  {
    // No main window (for example when a Python script is run without a GUI).
    return;
  }
  if (!layoutManager->mrmlViewFactory("vtkMRMLSequenceLightboxViewNode"))
  {
    layoutManager->registerViewFactory(new qMRMLLayoutSequenceLightboxViewFactory(layoutManager));
  }

  this->updateLightboxLayoutDescriptions();
  this->addLightboxLayoutMenuActions();
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModulePrivate::updateLightboxLayoutDescriptions()
{
  qSlicerApplication* app = qSlicerApplication::application();
  if (!app || !app->layoutManager() || !app->layoutManager()->layoutLogic())
  {
    return;
  }
  vtkMRMLLayoutNode* layoutNode = app->layoutManager()->layoutLogic()->GetLayoutNode();
  if (!layoutNode)
  {
    return;
  }
  vtkMRMLScene* scene = app->layoutManager()->mrmlScene();

  const std::string redSliceView = viewLayoutDescription("vtkMRMLSliceNode",
                                                         "Red",
                                                         "R",
                                                         "<property name=\"orientation\" action=\"default\">Axial</property>"
                                                         "<property name=\"viewcolor\" action=\"default\">#F34A33</property>");
  const std::string threeDView = viewLayoutDescription("vtkMRMLViewNode", "1", "1");
  const std::string sliceLightbox = viewLayoutDescription("vtkMRMLSequenceLightboxViewNode", qSlicerSequenceLightboxModule::SequenceLightboxSliceViewLayoutName, "L");
  const std::string threeDLightbox = viewLayoutDescription("vtkMRMLSequenceLightboxViewNode", qSlicerSequenceLightboxModule::SequenceLightbox3DViewLayoutName, "L");
  const std::string sliceTimeline = viewLayoutDescription("vtkMRMLSequenceLightboxViewNode", qSlicerSequenceLightboxModule::SequenceTimelineSliceViewLayoutName, "T");
  const std::string threeDTimeline = viewLayoutDescription("vtkMRMLSequenceLightboxViewNode", qSlicerSequenceLightboxModule::SequenceTimeline3DViewLayoutName, "T");

  // The view that is displayed next to a lightbox is the reference view of the lightbox (the red
  // slice view or the first 3D view until the user chooses another one).
  const std::string sliceLightboxCompanion =
    companionViewLayoutDescription(scene, qSlicerSequenceLightboxModule::SequenceLightboxSliceViewLayoutName, "vtkMRMLSliceNode", "Red", redSliceView);
  const std::string threeDLightboxCompanion =
    companionViewLayoutDescription(scene, qSlicerSequenceLightboxModule::SequenceLightbox3DViewLayoutName, "vtkMRMLViewNode", "1", threeDView);
  const std::string sliceTimelineCompanion =
    companionViewLayoutDescription(scene, qSlicerSequenceLightboxModule::SequenceTimelineSliceViewLayoutName, "vtkMRMLSliceNode", "Red", redSliceView);
  const std::string threeDTimelineCompanion =
    companionViewLayoutDescription(scene, qSlicerSequenceLightboxModule::SequenceTimeline3DViewLayoutName, "vtkMRMLViewNode", "1", threeDView);

  // A single modified event (if anything changed), so that the layout manager rebuilds the
  // current layout at most once.
  MRMLNodeModifyBlocker blocker(layoutNode);
  auto setLayoutDescription = [layoutNode](int layout, const std::string& description)
  {
    if (layoutNode->IsLayoutDescription(layout))
    {
      // Only modifies the layout node if the description is different
      layoutNode->SetLayoutDescription(layout, description.c_str());
    }
    else
    {
      layoutNode->AddLayoutDescription(layout, description.c_str());
    }
  };
  // Lightbox next to the view that it follows.
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxSliceView, splitLayoutDescription("horizontal", sliceLightboxCompanion, 400, sliceLightbox, 600));
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightbox3DView, splitLayoutDescription("horizontal", threeDLightboxCompanion, 400, threeDLightbox, 600));
  // Timeline: the view on top, a strip of sequence items along the bottom.
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimelineSliceView, splitLayoutDescription("vertical", sliceTimelineCompanion, 700, sliceTimeline, 300));
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimeline3DView, splitLayoutDescription("vertical", threeDTimelineCompanion, 700, threeDTimeline, 300));
  // Lightbox only: the tiles are interactive, so the view that they follow does not have to be
  // displayed. The same lightbox views are used as in the side-by-side layouts, so switching
  // between the two keeps the lightbox settings.
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnlySliceView, singleViewLayoutDescription(sliceLightbox));
  setLayoutDescription(qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnly3DView, singleViewLayoutDescription(threeDLightbox));
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModulePrivate::addLightboxLayoutMenuActions()
{
  if (this->LightboxLayoutMenuActionsAdded)
  {
    return;
  }
  QMainWindow* mainWindow = qSlicerApplication::application()->mainWindow();
  if (!mainWindow)
  {
    return;
  }
  QMenu* layoutMenu = mainWindow->findChild<QMenu*>("LayoutMenu");
  if (!layoutMenu)
  {
    qWarning("qSlicerSequenceLightboxModule: layout menu is not found, sequence lightbox layouts are not added to it");
    return;
  }
  // The actions must be added to the layout menu directly (not to a submenu), because the
  // main window only looks for the triggered layout among the top level actions of the menu.
  layoutMenu->addSeparator();

  struct LayoutAction
  {
    int Layout;
    QString Text;
    QString ToolTip;
  };
  const LayoutAction layoutActions[] = {
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxSliceView,
      qSlicerSequenceLightboxModule::tr("Sequence lightbox (slice)"),
      qSlicerSequenceLightboxModule::tr("Show a slice view and a grid of sequence items of that slice view.") },
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightbox3DView,
      qSlicerSequenceLightboxModule::tr("Sequence lightbox (3D)"),
      qSlicerSequenceLightboxModule::tr("Show a 3D view and a grid of sequence items of that 3D view.") },
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimelineSliceView,
      qSlicerSequenceLightboxModule::tr("Sequence timeline (slice)"),
      qSlicerSequenceLightboxModule::tr("Show a slice view with a strip of the sequence items around the current item along the bottom.") },
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceTimeline3DView,
      qSlicerSequenceLightboxModule::tr("Sequence timeline (3D)"),
      qSlicerSequenceLightboxModule::tr("Show a 3D view with a strip of the sequence items around the current item along the bottom.") },
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnlySliceView,
      qSlicerSequenceLightboxModule::tr("Sequence lightbox only (slice)"),
      qSlicerSequenceLightboxModule::tr("Show only a grid of sequence items of the red slice view. The tiles can be interacted with like slice views.") },
    { qSlicerSequenceLightboxModule::SlicerLayoutSequenceLightboxOnly3DView,
      qSlicerSequenceLightboxModule::tr("Sequence lightbox only (3D)"),
      qSlicerSequenceLightboxModule::tr("Show only a grid of sequence items of the first 3D view. The tiles can be interacted with like 3D views.") },
  };
  for (const LayoutAction& layoutAction : layoutActions)
  {
    QAction* action = new QAction(layoutAction.Text, layoutMenu);
    action->setToolTip(layoutAction.ToolTip);
    action->setData(layoutAction.Layout);
    layoutMenu->addAction(action);
  }

  this->LightboxLayoutMenuActionsAdded = true;
}

//-----------------------------------------------------------------------------
// qSlicerSequenceLightboxModule methods

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModule::qSlicerSequenceLightboxModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerSequenceLightboxModulePrivate)
{
  Q_D(qSlicerSequenceLightboxModule);
  d->UpdateLightboxLayoutDescriptionsTimer.setSingleShot(true);
  d->UpdateLightboxLayoutDescriptionsTimer.setInterval(0);
  connect(&d->UpdateLightboxLayoutDescriptionsTimer, SIGNAL(timeout()), this, SLOT(onUpdateLightboxLayoutDescriptionsTimeout()));
}

//-----------------------------------------------------------------------------
qSlicerSequenceLightboxModule::~qSlicerSequenceLightboxModule() = default;

//-----------------------------------------------------------------------------
QString qSlicerSequenceLightboxModule::helpText() const
{
  return tr("Display several items of a sequence at once: the lightbox view shows a grid of sequence items that follow"
            " a slice view or a 3D view, the timeline view shows a strip of the items around the current item."
            " The tiles can be interacted with like regular views, without adding anything to the scene.");
}

//-----------------------------------------------------------------------------
QString qSlicerSequenceLightboxModule::acknowledgementText() const
{
  return tr("This work was partially funded by the Natural Sciences and Engineering Research Council of Canada (NSERC).");
}

//-----------------------------------------------------------------------------
QStringList qSlicerSequenceLightboxModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Kyle Sunderland (Radixlab Medical Inc.)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerSequenceLightboxModule::icon() const
{
  return QIcon(":/Icons/SequenceLightbox.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerSequenceLightboxModule::categories() const
{
  return QStringList() << "Sequences";
}

//-----------------------------------------------------------------------------
QStringList qSlicerSequenceLightboxModule::dependencies() const
{
  return QStringList() << "Sequences";
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::setup()
{
  Q_D(qSlicerSequenceLightboxModule);
  this->Superclass::setup();
  d->registerLightboxView();
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerSequenceLightboxModule::createWidgetRepresentation()
{
  return new qSlicerSequenceLightboxModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerSequenceLightboxModule::createLogic()
{
  return vtkSlicerSequenceLightboxLogic::New();
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerSequenceLightboxModule);
  vtkMRMLScene* oldScene = this->mrmlScene();
  this->Superclass::setMRMLScene(scene);

  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(onNodeAddedEvent(vtkObject*, vtkObject*)));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(onNodeRemovedEvent(vtkObject*, vtkObject*)));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onMRMLSceneEndCloseEvent()));
  this->qvtkReconnect(oldScene, scene, vtkMRMLScene::EndImportEvent, this, SLOT(onMRMLSceneEndImportEvent()));

  // The reference views of the lightbox view nodes determine the lightbox layouts.
  std::vector<vtkMRMLNode*> lightboxViewNodes;
  if (oldScene)
  {
    oldScene->GetNodesByClass("vtkMRMLSequenceLightboxViewNode", lightboxViewNodes);
    for (vtkMRMLNode* lightboxViewNode : lightboxViewNodes)
    {
      this->qvtkDisconnect(lightboxViewNode, vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, this, SLOT(updateLightboxLayoutDescriptions()));
    }
    lightboxViewNodes.clear();
  }
  if (scene)
  {
    scene->GetNodesByClass("vtkMRMLSequenceLightboxViewNode", lightboxViewNodes);
    for (vtkMRMLNode* lightboxViewNode : lightboxViewNodes)
    {
      this->qvtkConnect(lightboxViewNode, vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, this, SLOT(updateLightboxLayoutDescriptions()));
    }
  }

  // A new scene has a new layout node, the lightbox layouts must be registered in it again.
  d->updateLightboxLayoutDescriptions();
}

// --------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::onMRMLSceneEndImportEvent()
{
  Q_D(qSlicerSequenceLightboxModule);
  // Lightbox views of the imported scene that reference nodes that were not imported (or lightbox
  // views that were created by the layout of the imported scene) need a usable configuration.
  qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(this->mrmlScene());
  // The reference views of the imported lightbox views determine the lightbox layouts.
  d->updateLightboxLayoutDescriptions();
}

// --------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::onMRMLSceneEndCloseEvent()
{
  Q_D(qSlicerSequenceLightboxModule);
  // Lightbox view nodes are singletons: they survive closing the scene, but they are reset to
  // their defaults, which removes their references to the sequence browser and to the reference
  // view, and turns the timeline views into regular grids.
  qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(this->mrmlScene());
  if (this->mrmlScene())
  {
    std::vector<vtkMRMLNode*> viewNodes;
    this->mrmlScene()->GetNodesByClass("vtkMRMLSequenceLightboxViewNode", viewNodes);
    for (vtkMRMLNode* node : viewNodes)
    {
      vtkMRMLSequenceLightboxViewNode* viewNode = vtkMRMLSequenceLightboxViewNode::SafeDownCast(node);
      if (viewNode && qMRMLLayoutSequenceLightboxViewFactory::isTimelineLayoutName(viewNode->GetLayoutName()))
      {
        qMRMLLayoutSequenceLightboxViewFactory::applyTimelineConfiguration(viewNode);
      }
    }
  }
  // The layout node is reset as well: the lightbox layouts must be registered again.
  d->updateLightboxLayoutDescriptions();
}

// --------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::onNodeAddedEvent(vtkObject*, vtkObject* node)
{
  if (vtkMRMLSequenceLightboxViewNode::SafeDownCast(node))
  {
    // The view that is displayed next to the lightbox follows the reference view of the node.
    this->qvtkConnect(node, vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, this, SLOT(updateLightboxLayoutDescriptions()));
    this->updateLightboxLayoutDescriptions();
    return;
  }

  if (!vtkMRMLSequenceBrowserNode::SafeDownCast(node) || !this->mrmlScene())
  {
    return;
  }
  // A lightbox view that has no sequence browser node to show can show this one. While a scene
  // is imported the node references are not resolved yet, so the lightbox views would appear
  // unconfigured and their references from the scene file would be overwritten: they are
  // configured when the import is finished instead.
  if (!this->mrmlScene()->IsImporting() && !this->mrmlScene()->IsRestoring())
  {
    qMRMLLayoutSequenceLightboxViewFactory::applyDefaultConfiguration(this->mrmlScene());
  }
}

// --------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::onNodeRemovedEvent(vtkObject*, vtkObject* node)
{
  if (vtkMRMLSequenceLightboxViewNode::SafeDownCast(node))
  {
    this->qvtkDisconnect(node, vtkMRMLSequenceLightboxViewNode::ReferenceViewNodeModifiedEvent, this, SLOT(updateLightboxLayoutDescriptions()));
    this->updateLightboxLayoutDescriptions();
  }
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::updateLightboxLayoutDescriptions()
{
  Q_D(qSlicerSequenceLightboxModule);
  // Not updated immediately: the layout node must not be modified while the layout logic
  // iterates over the current layout (it creates the lightbox view node, which sets its
  // reference view) or while the layout manager applies a layout.
  d->UpdateLightboxLayoutDescriptionsTimer.start();
}

//-----------------------------------------------------------------------------
void qSlicerSequenceLightboxModule::onUpdateLightboxLayoutDescriptionsTimeout()
{
  Q_D(qSlicerSequenceLightboxModule);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene || scene->IsImporting() || scene->IsRestoring() || scene->IsClosing())
  {
    // The node references are not resolved yet (or the nodes are being reset): the layouts are
    // updated when the import (or the closing) of the scene is finished.
    return;
  }
  d->updateLightboxLayoutDescriptions();
}
