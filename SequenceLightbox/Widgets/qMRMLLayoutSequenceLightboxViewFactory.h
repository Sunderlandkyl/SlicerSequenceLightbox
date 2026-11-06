/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __qMRMLLayoutSequenceLightboxViewFactory_h
#define __qMRMLLayoutSequenceLightboxViewFactory_h

// qMRMLWidgets includes
#include <qMRMLLayoutViewFactory.h>

#include "qSlicerSequenceLightboxModuleWidgetsExport.h"

class vtkMRMLAbstractViewNode;
class vtkMRMLScene;
class vtkMRMLSequenceLightboxViewNode;

/// \brief Creates the widget of sequence lightbox view nodes in the layout.
///
/// Register it in the layout manager to make sequence lightbox views available in layouts:
/// \code
/// layoutManager->registerViewFactory(new qMRMLLayoutSequenceLightboxViewFactory(layoutManager));
/// \endcode
class Q_SLICER_MODULE_SEQUENCELIGHTBOX_WIDGETS_EXPORT qMRMLLayoutSequenceLightboxViewFactory : public qMRMLLayoutViewFactory
{
  Q_OBJECT

public:
  typedef qMRMLLayoutViewFactory Superclass;

  explicit qMRMLLayoutSequenceLightboxViewFactory(QObject* parent = nullptr);
  ~qMRMLLayoutSequenceLightboxViewFactory() override;

  QString viewClassName() const override;

  /// Set the sequence browser node and the reference view node of a lightbox view node that
  /// does not have them, so that the view shows something meaningful without any user
  /// interaction. Already configured view nodes are left unchanged.
  ///
  /// A lightbox view node is a singleton, therefore it survives closing the scene, but its node
  /// references do not. This method is called when a lightbox view is created, and by the
  /// Sequences module when the scene is closed or a node is added.
  static void applyDefaultConfiguration(vtkMRMLScene* scene, vtkMRMLSequenceLightboxViewNode* viewNode);

  /// Apply the default configuration to every lightbox view node of the scene.
  /// \sa applyDefaultConfiguration
  static void applyDefaultConfiguration(vtkMRMLScene* scene);

  /// Returns true if the layout name (singleton tag) belongs to a lightbox view that is displayed
  /// as a timeline strip: a single row of tiles centered on the selected item, below the view
  /// that the tiles follow. By convention these layout names start with "SequenceTimeline".
  static bool isTimelineLayoutName(const char* layoutName);

  /// Configure the view node as a timeline strip: a single row of tiles, centered on the item that
  /// is selected in the browser node. Called once, when the view of a timeline lightbox view node
  /// is created (nodes loaded from a scene file keep their settings).
  static void applyTimelineConfiguration(vtkMRMLSequenceLightboxViewNode* viewNode);

protected:
  QWidget* createViewFromNode(vtkMRMLAbstractViewNode* viewNode) override;

private:
  Q_DISABLE_COPY(qMRMLLayoutSequenceLightboxViewFactory);
};

#endif
