/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

#ifndef __vtkMRMLSequenceLightboxViewDisplayableManagerFactory_h
#define __vtkMRMLSequenceLightboxViewDisplayableManagerFactory_h

// MRMLDisplayableManager includes
#include "vtkMRMLDisplayableManagerFactory.h"

// VTK includes
#include <vtkSingleton.h>

#include "vtkSlicerSequenceLightboxModuleMRMLDisplayableManagerExport.h"

class vtkRenderer;

/// \brief Factory where displayable managers of sequence lightbox views are registered.
///
/// Note that this factory only holds the displayable manager that manages the tiles of the
/// lightbox view. The content of the tiles themselves is rendered by the standard slice view
/// and 3D view displayable managers.
class VTK_SLICER_SEQUENCELIGHTBOX_MODULE_MRMLDISPLAYABLEMANAGER_EXPORT vtkMRMLSequenceLightboxViewDisplayableManagerFactory : public vtkMRMLDisplayableManagerFactory
{
public:
  vtkTypeMacro(vtkMRMLSequenceLightboxViewDisplayableManagerFactory, vtkMRMLDisplayableManagerFactory);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// This is a singleton pattern New. There will only be ONE reference to a
  /// vtkMRMLSequenceLightboxViewDisplayableManagerFactory object per process. Clients that
  /// call this must call Delete on the object so that the reference counting will work.
  /// The single instance will be unreferenced when the program exits.
  static vtkMRMLSequenceLightboxViewDisplayableManagerFactory* New();

  /// Return the singleton instance with no reference counting.
  static vtkMRMLSequenceLightboxViewDisplayableManagerFactory* GetInstance();

  /// Register the VTK object factory of this library.
  ///
  /// Displayable managers are instantiated by class name (using
  /// vtkObjectFactory::CreateInstance), which only works for classes that a registered VTK
  /// object factory can create. This method registers the object factory that is generated
  /// for this library. It is called by the constructor of this factory, so it usually does
  /// not need to be called explicitly. Calling it multiple times has no effect.
  static void RegisterObjectFactory();

protected:
  vtkMRMLSequenceLightboxViewDisplayableManagerFactory();
  ~vtkMRMLSequenceLightboxViewDisplayableManagerFactory() override;

  VTK_SINGLETON_DECLARE(vtkMRMLSequenceLightboxViewDisplayableManagerFactory);

private:
  vtkMRMLSequenceLightboxViewDisplayableManagerFactory(const vtkMRMLSequenceLightboxViewDisplayableManagerFactory&) = delete;
  void operator=(const vtkMRMLSequenceLightboxViewDisplayableManagerFactory&) = delete;
};

#ifndef __VTK_WRAP__
VTK_SINGLETON_DECLARE_INITIALIZER(VTK_SLICER_SEQUENCELIGHTBOX_MODULE_MRMLDISPLAYABLEMANAGER_EXPORT, vtkMRMLSequenceLightboxViewDisplayableManagerFactory);
#endif // __VTK_WRAP__

#endif
