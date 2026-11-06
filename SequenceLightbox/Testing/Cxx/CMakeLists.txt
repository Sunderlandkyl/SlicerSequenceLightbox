set(KIT qSlicer${MODULE_NAME}Module)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  vtkMRMLSequenceLightboxViewNodeTest1.cxx
  vtkSequenceLightboxSceneMirrorTest1.cxx
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  INCLUDE_DIRECTORIES
    ${vtkSlicerSequencesModuleLogic_INCLUDE_DIRS}
    ${vtkSlicerSequencesModuleMRML_INCLUDE_DIRS}
  TARGET_LIBRARIES
    vtkSlicerSequencesModuleLogic
  WITH_VTK_DEBUG_LEAKS_CHECK
  )

#-----------------------------------------------------------------------------
simple_test(vtkMRMLSequenceLightboxViewNodeTest1)
simple_test(vtkSequenceLightboxSceneMirrorTest1)
