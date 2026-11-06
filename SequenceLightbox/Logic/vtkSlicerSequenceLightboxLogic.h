/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __vtkSlicerSequenceLightboxLogic_h
#define __vtkSlicerSequenceLightboxLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"

#include "vtkSlicerSequenceLightboxModuleLogicExport.h"

/// \brief Logic of the Sequence Lightbox module.
///
/// Registers the sequence lightbox view node class in the scene. The lightbox views themselves
/// are created by the layout manager (see qMRMLLayoutSequenceLightboxViewFactory) and their
/// content is managed by vtkMRMLSequenceLightboxDisplayableManager.
class VTK_SLICER_SEQUENCELIGHTBOX_MODULE_LOGIC_EXPORT vtkSlicerSequenceLightboxLogic : public vtkSlicerModuleLogic
{
public:
  static vtkSlicerSequenceLightboxLogic* New();
  vtkTypeMacro(vtkSlicerSequenceLightboxLogic, vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

protected:
  vtkSlicerSequenceLightboxLogic();
  ~vtkSlicerSequenceLightboxLogic() override;

  /// Register the sequence lightbox view node class in the scene.
  void RegisterNodes() override;

private:
  vtkSlicerSequenceLightboxLogic(const vtkSlicerSequenceLightboxLogic&) = delete;
  void operator=(const vtkSlicerSequenceLightboxLogic&) = delete;
};

#endif
