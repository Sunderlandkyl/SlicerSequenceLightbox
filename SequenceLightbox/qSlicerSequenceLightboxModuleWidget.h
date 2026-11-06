/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qSlicerSequenceLightboxModuleWidget_h
#define __qSlicerSequenceLightboxModuleWidget_h

// Slicer includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerSequenceLightboxModuleExport.h"

class qSlicerSequenceLightboxModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLScene;
class vtkMRMLSequenceLightboxViewNode;

/// \brief Panel of the Sequence Lightbox module.
///
/// Switches to a lightbox or timeline layout and sets the main properties of the lightbox view
/// that is displayed (sequence browser node, reference view, number of rows and columns). The
/// other settings of the view are in the controller bar of the view.
class Q_SLICER_QTMODULES_SEQUENCELIGHTBOX_EXPORT qSlicerSequenceLightboxModuleWidget : public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:
  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerSequenceLightboxModuleWidget(QWidget* parent = nullptr);
  ~qSlicerSequenceLightboxModuleWidget() override;

  /// Layout that the "Show" button switches to.
  Q_INVOKABLE int selectedLightboxLayout();

  /// Lightbox view node that the panel controls: the lightbox view that is currently displayed,
  /// if there is one, otherwise the lightbox view of the selected layout (if it exists already).
  Q_INVOKABLE vtkMRMLSequenceLightboxViewNode* lightboxViewNode();

public slots:
  void setMRMLScene(vtkMRMLScene* scene) override;

  /// Switch to the lightbox layout that is selected in the panel, and show the selected
  /// sequence browser node in it.
  void onShowLightboxButtonClicked();

protected slots:
  /// Set the sequence browser node whose items the lightbox displays.
  void onSequenceBrowserNodeChanged(vtkMRMLNode* node);
  /// Set the view that the tiles of the lightbox follow.
  void onLightboxReferenceViewNodeChanged(vtkMRMLNode* node);
  void onLightboxRowsChanged(int rows);
  void onLightboxColumnsChanged(int columns);
  /// Update the panel from the lightbox view node.
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qSlicerSequenceLightboxModuleWidgetPrivate> d_ptr;

  void setup() override;
  void enter() override;

private:
  Q_DECLARE_PRIVATE(qSlicerSequenceLightboxModuleWidget);
  Q_DISABLE_COPY(qSlicerSequenceLightboxModuleWidget);
};

#endif
