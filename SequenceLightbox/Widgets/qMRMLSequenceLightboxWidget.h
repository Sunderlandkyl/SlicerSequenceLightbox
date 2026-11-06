/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qMRMLSequenceLightboxWidget_h
#define __qMRMLSequenceLightboxWidget_h

// qMRMLWidgets includes
#include <qMRMLAbstractViewWidget.h>

#include "qSlicerSequenceLightboxModuleWidgetsExport.h"

class qMRMLSequenceLightboxView;
class vtkMRMLAbstractDisplayableManager;
class qMRMLSequenceLightboxViewControllerWidget;
class qMRMLSequenceLightboxWidgetPrivate;
class vtkMRMLScene;
class vtkMRMLSequenceLightboxViewNode;

/// \brief Top-level widget of a sequence lightbox view, to be packed in a layout.
///
/// It contains a controller bar and the view that displays the grid of sequence items.
class Q_SLICER_MODULE_SEQUENCELIGHTBOX_WIDGETS_EXPORT qMRMLSequenceLightboxWidget : public qMRMLAbstractViewWidget
{
  Q_OBJECT

public:
  typedef qMRMLAbstractViewWidget Superclass;

  explicit qMRMLSequenceLightboxWidget(QWidget* parent = nullptr);
  ~qMRMLSequenceLightboxWidget() override;

  /// Get the lightbox view node observed by the view.
  Q_INVOKABLE vtkMRMLSequenceLightboxViewNode* mrmlSequenceLightboxViewNode() const;
  vtkMRMLAbstractViewNode* mrmlAbstractViewNode() const override;

  /// Get the lightbox view.
  Q_INVOKABLE qMRMLSequenceLightboxView* sequenceLightboxView() const;
  QWidget* viewWidget() const override;

  /// Get the controller bar of the view.
  Q_INVOKABLE qMRMLSequenceLightboxViewControllerWidget* sequenceLightboxController() const;
  qMRMLViewControllerBar* controllerWidget() const override;

  /// Get a displayable manager of the view given its class name.
  /// Provided here (and not only in the view) because the view class is not accessible
  /// from Python.
  Q_INVOKABLE vtkMRMLAbstractDisplayableManager* displayableManagerByClassName(const char* className);

public slots:
  /// Resize the grid so that every item of the sequence is displayed (consecutive items from the
  /// first item), with tiles as large as the size of the view allows.
  void showAllItems();
  /// Set the current view node to observe.
  void setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* newViewNode);
  void setMRMLAbstractViewNode(vtkMRMLAbstractViewNode* newViewNode) override;

protected:
  QScopedPointer<qMRMLSequenceLightboxWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLSequenceLightboxWidget);
  Q_DISABLE_COPY(qMRMLSequenceLightboxWidget);
};

#endif
