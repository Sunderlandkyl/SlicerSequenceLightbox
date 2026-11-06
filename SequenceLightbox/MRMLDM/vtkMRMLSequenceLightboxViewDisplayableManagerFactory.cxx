/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// Sequences MRMLDM includes
#include "vtkMRMLSequenceLightboxViewDisplayableManagerFactory.h"
#include "vtkSlicerSequenceLightboxModuleMRMLDisplayableManagerObjectFactory.h"

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>

//----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewDisplayableManagerFactory::vtkMRMLSequenceLightboxViewDisplayableManagerFactory()
  : Superclass()
{
  vtkMRMLSequenceLightboxViewDisplayableManagerFactory::RegisterObjectFactory();
}

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewDisplayableManagerFactory::RegisterObjectFactory()
{
  static bool objectFactoryRegistered = false;
  if (objectFactoryRegistered)
  {
    return;
  }
  vtkNew<vtkSlicerSequenceLightboxModuleMRMLDisplayableManagerObjectFactory> objectFactory;
  // The registry keeps a reference to the object factory.
  vtkObjectFactory::RegisterFactory(objectFactory);
  objectFactoryRegistered = true;
}

//----------------------------------------------------------------------------
vtkMRMLSequenceLightboxViewDisplayableManagerFactory::~vtkMRMLSequenceLightboxViewDisplayableManagerFactory() = default;

//----------------------------------------------------------------------------
void vtkMRMLSequenceLightboxViewDisplayableManagerFactory::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

VTK_SINGLETON_CXX(vtkMRMLSequenceLightboxViewDisplayableManagerFactory);
