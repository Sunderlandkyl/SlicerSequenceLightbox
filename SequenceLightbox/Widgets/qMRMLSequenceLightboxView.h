/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qMRMLSequenceLightboxView_h
#define __qMRMLSequenceLightboxView_h

// CTK includes
#include <ctkVTKObject.h>
#include <ctkVTKRenderView.h>

#include "qSlicerSequenceLightboxModuleWidgetsExport.h"

class qMRMLSequenceLightboxViewPrivate;
class vtkMRMLAbstractDisplayableManager;
class vtkMRMLDisplayableManagerGroup;
class vtkMRMLScene;
class vtkMRMLSequenceLightboxViewNode;
class vtkMRMLViewInteractorStyle;

/// \brief View that displays a grid of sequence items.
///
/// The content of the tiles is rendered by vtkMRMLSequenceLightboxDisplayableManager, which
/// creates a complete, independent view (renderer, scene, displayable managers) for each tile.
/// \sa qMRMLSequenceLightboxWidget, vtkMRMLSequenceLightboxViewNode
class Q_SLICER_MODULE_SEQUENCELIGHTBOX_WIDGETS_EXPORT qMRMLSequenceLightboxView : public ctkVTKRenderView
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef ctkVTKRenderView Superclass;

  explicit qMRMLSequenceLightboxView(QWidget* parent = nullptr);
  ~qMRMLSequenceLightboxView() override;

  /// Sets the interactor of the view.
  void setInteractor(vtkRenderWindowInteractor* interactor) override;

  /// Returns the interactor observer of the view.
  Q_INVOKABLE vtkMRMLViewInteractorStyle* interactorObserver() const;

  /// Get the lightbox view node observed by this view.
  Q_INVOKABLE vtkMRMLSequenceLightboxViewNode* mrmlSequenceLightboxViewNode() const;

  /// Return a displayable manager of this view given its class name.
  Q_INVOKABLE vtkMRMLAbstractDisplayableManager* displayableManagerByClassName(const char* className);

  /// Group of displayable managers of this view.
  Q_INVOKABLE vtkMRMLDisplayableManagerGroup* displayableManagerGroup() const;

public slots:

  /// Set the MRML scene that should be listened for events.
  void setMRMLScene(vtkMRMLScene* newScene);

  /// Synchronize the tiles with the scene, then render the view.
  void forceRender() override;

  /// Set the current view node to observe.
  void setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* newViewNode);

protected slots:
  void onSceneStartProcessing();
  void onSceneEndProcessing();
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qMRMLSequenceLightboxViewPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLSequenceLightboxView);
  Q_DISABLE_COPY(qMRMLSequenceLightboxView);
};

#endif
