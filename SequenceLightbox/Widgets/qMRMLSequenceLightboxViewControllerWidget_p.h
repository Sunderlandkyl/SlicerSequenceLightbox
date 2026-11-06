/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qMRMLSequenceLightboxViewControllerWidget_p_h
#define __qMRMLSequenceLightboxViewControllerWidget_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// qMRML includes
#include <qMRMLViewControllerBar_p.h>

// Sequences includes
#include "qMRMLSequenceLightboxViewControllerWidget.h"

// VTK includes
#include <vtkWeakPointer.h>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class qMRMLNodeComboBox;
class vtkMRMLSequenceLightboxViewNode;

//-----------------------------------------------------------------------------
class qMRMLSequenceLightboxViewControllerWidgetPrivate : public qMRMLViewControllerBarPrivate
{
  Q_OBJECT
  Q_DECLARE_PUBLIC(qMRMLSequenceLightboxViewControllerWidget);

protected:
  qMRMLSequenceLightboxViewControllerWidget* const q_ptr;

  /// Fill the popup of the controller bar with the settings that are used less often.
  void setupPopupUi() override;

public:
  typedef qMRMLViewControllerBarPrivate Superclass;
  qMRMLSequenceLightboxViewControllerWidgetPrivate(qMRMLSequenceLightboxViewControllerWidget& object);
  ~qMRMLSequenceLightboxViewControllerWidgetPrivate() override;

  void init() override;

  /// The lightbox view node that is controlled by this widget.
  /// The base class stores it as a vtkMRMLAbstractViewNode in ViewNode.
  vtkWeakPointer<vtkMRMLSequenceLightboxViewNode> LightboxViewNode;

  /// Bar: the settings that are changed most often.
  qMRMLNodeComboBox* SequenceBrowserNodeSelector{ nullptr };
  qMRMLNodeComboBox* ReferenceViewNodeSelector{ nullptr };
  QSpinBox* RowsSpinBox{ nullptr };
  QSpinBox* ColumnsSpinBox{ nullptr };
  QSpinBox* TileSpacingSpinBox{ nullptr };
  QPushButton* ShowAllItemsButton{ nullptr };

  /// Popup: the settings that are changed less often.
  QComboBox* ItemSelectionModeComboBox{ nullptr };
  QSpinBox* ItemStepSpinBox{ nullptr };
  QSpinBox* StartItemNumberSpinBox{ nullptr };
  QLineEdit* CustomItemNumbersLineEdit{ nullptr };
  QCheckBox* ShowItemLabelsCheckBox{ nullptr };
  QCheckBox* HighlightSelectedItemCheckBox{ nullptr };
  QCheckBox* InteractiveTilesCheckBox{ nullptr };

  bool UpdatingWidgetFromMRML{ false };
};

#endif
