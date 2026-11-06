/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qSlicerSequenceLightboxModule_h
#define __qSlicerSequenceLightboxModule_h

// CTK includes
#include <ctkVTKObject.h>

// Slicer includes
#include "qSlicerLoadableModule.h"

// MRML includes
#include <vtkMRMLLayoutNode.h>

#include "qSlicerSequenceLightboxModuleExport.h"

class qSlicerSequenceLightboxModulePrivate;
class vtkMRMLScene;
class vtkObject;

/// \brief Sequence Lightbox module.
///
/// Makes the sequence lightbox view available in the application: registers the view node
/// factory in the layout manager and the displayable manager of the view, adds the lightbox
/// and timeline layouts to the layout node and to the layout menu, and keeps the lightbox view
/// nodes configured (sequence browser node, reference view) when nodes are added or a scene is
/// loaded or closed.
class Q_SLICER_QTMODULES_SEQUENCELIGHTBOX_EXPORT qSlicerSequenceLightboxModule : public qSlicerLoadableModule
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PLUGIN_METADATA(IID "org.slicer.modules.loadable.qSlicerLoadableModule/1.0");
  Q_INTERFACES(qSlicerLoadableModule);

public:
  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerSequenceLightboxModule(QObject* parent = nullptr);
  ~qSlicerSequenceLightboxModule() override;

  qSlicerGetTitleMacro(tr("Sequence Lightbox"));

  QString helpText() const override;
  QString acknowledgementText() const override;
  QStringList contributors() const override;

  QIcon icon() const override;

  QStringList categories() const override;
  QStringList dependencies() const override;

  /// Layout identifiers of the layouts that contain a sequence lightbox view.
  /// They are registered by this module, therefore they are not part of the standard
  /// vtkMRMLLayoutNode layout list.
  enum
  {
    /// A slice view and a lightbox that tiles the slice view.
    SlicerLayoutSequenceLightboxSliceView = vtkMRMLLayoutNode::SlicerLayoutUserView + 601,
    /// A 3D view and a lightbox that tiles the 3D view.
    SlicerLayoutSequenceLightbox3DView,
    /// A slice view on top and a timeline strip of sequence items of that slice view below it.
    SlicerLayoutSequenceTimelineSliceView,
    /// A 3D view on top and a timeline strip of sequence items of that 3D view below it.
    SlicerLayoutSequenceTimeline3DView,
    /// Only a lightbox that tiles the red slice view. The tiles are interactive, so the slice
    /// view itself does not have to be displayed.
    SlicerLayoutSequenceLightboxOnlySliceView,
    /// Only a lightbox that tiles the first 3D view.
    SlicerLayoutSequenceLightboxOnly3DView
  };

  /// Layout name (singleton tag) of the lightbox view of the slice lightbox layouts.
  /// The lightbox-only slice layout shows the same view, so its settings are shared.
  static const char* SequenceLightboxSliceViewLayoutName;
  /// Layout name (singleton tag) of the lightbox view of the 3D lightbox layouts.
  static const char* SequenceLightbox3DViewLayoutName;
  /// Layout name (singleton tag) of the lightbox view of the slice timeline layout.
  static const char* SequenceTimelineSliceViewLayoutName;
  /// Layout name (singleton tag) of the lightbox view of the 3D timeline layout.
  /// Lightbox views whose layout name starts with "SequenceTimeline" are configured as a timeline
  /// strip when they are created, see qMRMLLayoutSequenceLightboxViewFactory::isTimelineLayoutName.
  static const char* SequenceTimeline3DViewLayoutName;

public slots:
  void setMRMLScene(vtkMRMLScene* scene) override;

  /// Requests an update of the sequence lightbox layouts in the layout node of the scene: the
  /// view that is displayed next to a lightbox is the reference view of the lightbox view node.
  /// Called whenever the reference view of a lightbox view node changes. The layouts are
  /// updated when control returns to the event loop: the reference view may change while the
  /// layout logic or the layout manager applies a layout (for example when the lightbox view
  /// node is created by the layout), and the layouts must not change in the middle of that.
  void updateLightboxLayoutDescriptions();

protected slots:
  void onMRMLSceneEndCloseEvent();
  void onMRMLSceneEndImportEvent();
  void onNodeAddedEvent(vtkObject*, vtkObject*);
  void onNodeRemovedEvent(vtkObject*, vtkObject*);
  void onUpdateLightboxLayoutDescriptionsTimeout();

protected:
  /// Initialize the module: register the lightbox view, its displayable manager and its layouts.
  void setup() override;

  /// Create and return the widget representation associated to this module
  qSlicerAbstractModuleRepresentation* createWidgetRepresentation() override;

  /// Create and return the logic associated to this module
  vtkMRMLAbstractLogic* createLogic() override;

protected:
  QScopedPointer<qSlicerSequenceLightboxModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerSequenceLightboxModule);
  Q_DISABLE_COPY(qSlicerSequenceLightboxModule);
};

#endif
