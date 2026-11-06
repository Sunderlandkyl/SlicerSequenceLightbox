/*==============================================================================

  Copyright (c) Radixlab Medical Inc., Kingston, ON, Canada. All Rights Reserved.

  See LICENSE.txt or https://opensource.org/license/mit for details.

  The software is distributed under the MIT License on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, Radixlab Medical Inc.

==============================================================================*/

// SequenceLightbox includes
#include "vtkSlicerSequenceLightboxLogic.h"
#include "vtkMRMLSequenceLightboxViewNode.h"

// MRML includes
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerSequenceLightboxLogic);

//----------------------------------------------------------------------------
vtkSlicerSequenceLightboxLogic::vtkSlicerSequenceLightboxLogic() = default;

//----------------------------------------------------------------------------
vtkSlicerSequenceLightboxLogic::~vtkSlicerSequenceLightboxLogic() = default;

//----------------------------------------------------------------------------
void vtkSlicerSequenceLightboxLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerSequenceLightboxLogic::RegisterNodes()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
  {
    vtkErrorMacro("RegisterNodes: invalid scene");
    return;
  }
  vtkNew<vtkMRMLSequenceLightboxViewNode> lightboxViewNode;
  scene->RegisterNodeClass(lightboxViewNode);
}
