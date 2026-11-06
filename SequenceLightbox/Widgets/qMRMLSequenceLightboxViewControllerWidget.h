/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qMRMLSequenceLightboxViewControllerWidget_h
#define __qMRMLSequenceLightboxViewControllerWidget_h

// qMRMLWidgets includes
#include <qMRMLViewControllerBar.h>

#include "qSlicerSequenceLightboxModuleWidgetsExport.h"

class qMRMLSequenceLightboxViewControllerWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLScene;
class vtkMRMLSequenceLightboxViewNode;

/// \brief Controller bar of a sequence lightbox view.
///
/// Lets the user choose the tiled sequence browser, the view that the tiles follow, and the
/// layout of the grid.
class Q_SLICER_MODULE_SEQUENCELIGHTBOX_WIDGETS_EXPORT qMRMLSequenceLightboxViewControllerWidget : public qMRMLViewControllerBar
{
  Q_OBJECT

public:
  typedef qMRMLViewControllerBar Superclass;

  explicit qMRMLSequenceLightboxViewControllerWidget(QWidget* parent = nullptr);
  ~qMRMLSequenceLightboxViewControllerWidget() override;

  /// The lightbox view node that is controlled by this widget.
  Q_INVOKABLE vtkMRMLSequenceLightboxViewNode* mrmlSequenceLightboxViewNode() const;

public slots:
  void setMRMLScene(vtkMRMLScene* newScene) override;
  void setMRMLSequenceLightboxViewNode(vtkMRMLSequenceLightboxViewNode* viewNode);

  /// Set the label of the view, and store it in the view node.
  void setViewLabel(const QString& newViewLabel);

signals:
  /// Emitted when the user asks for a grid that shows all the items of the sequence. The view
  /// widget, which knows the size of the view, resizes the grid.
  void showAllItemsRequested();

protected slots:
  void updateWidgetFromMRMLView() override;

  void onSequenceBrowserNodeChanged(vtkMRMLNode* node);
  void onReferenceViewNodeChanged(vtkMRMLNode* node);
  void onRowsChanged(int rows);
  void onColumnsChanged(int columns);
  void onTileSpacingChanged(int tileSpacing);
  void onItemStepChanged(int itemStep);
  void onItemSelectionModeChanged(int index);
  void onStartItemNumberChanged(int startItemNumber);
  void onCustomItemNumbersChanged();
  void onShowItemLabelsToggled(bool show);
  void onHighlightSelectedItemToggled(bool highlight);
  void onInteractiveTilesToggled(bool interactive);

protected:
  void setMRMLViewNode(vtkMRMLAbstractViewNode* viewNode) override;

private:
  Q_DECLARE_PRIVATE(qMRMLSequenceLightboxViewControllerWidget);
  Q_DISABLE_COPY(qMRMLSequenceLightboxViewControllerWidget);
};

#endif
