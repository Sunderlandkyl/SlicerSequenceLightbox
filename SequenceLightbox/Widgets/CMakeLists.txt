project(qSlicer${MODULE_NAME}ModuleWidgets)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICER_MODULE_${MODULE_NAME_UPPER}_WIDGETS_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkSlicer${MODULE_NAME}ModuleMRML_SOURCE_DIR}
  ${vtkSlicer${MODULE_NAME}ModuleMRML_BINARY_DIR}
  ${vtkSlicer${MODULE_NAME}ModuleLogic_SOURCE_DIR}
  ${vtkSlicer${MODULE_NAME}ModuleLogic_BINARY_DIR}
  ${vtkSlicer${MODULE_NAME}ModuleMRMLDisplayableManager_SOURCE_DIR}
  ${vtkSlicer${MODULE_NAME}ModuleMRMLDisplayableManager_BINARY_DIR}
  ${vtkSlicerSequencesModuleMRML_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  qMRMLLayoutSequenceLightboxViewFactory.cxx
  qMRMLLayoutSequenceLightboxViewFactory.h
  qMRMLSequenceLightboxView.cxx
  qMRMLSequenceLightboxView.h
  qMRMLSequenceLightboxViewControllerWidget.cxx
  qMRMLSequenceLightboxViewControllerWidget.h
  qMRMLSequenceLightboxViewControllerWidget_p.h
  qMRMLSequenceLightboxWidget.cxx
  qMRMLSequenceLightboxWidget.h
  )

set(${KIT}_UI_SRCS
  )

set(${KIT}_RESOURCES
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicer${MODULE_NAME}ModuleLogic
  vtkSlicer${MODULE_NAME}ModuleMRML
  vtkSlicer${MODULE_NAME}ModuleMRMLDisplayableManager
  vtkSlicerSequencesModuleMRML
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleWidgets(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  UI_SRCS ${${KIT}_UI_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  RESOURCES ${${KIT}_RESOURCES}
  WRAP_PYTHONQT
  )
