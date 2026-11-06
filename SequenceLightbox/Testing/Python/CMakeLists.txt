if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME SequenceLightboxSelfTest
    SCRIPTS SequenceLightboxSelfTest.py
    )

  # The test runs in the Slicer application of the Slicer build tree, which does not know the
  # modules of this extension: they are made available with the additional module paths.
  if(CMAKE_CONFIGURATION_TYPES)
    set(MODULE_BUILD_DIR "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}/$<CONFIG>")
  else()
    set(MODULE_BUILD_DIR "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}")
  endif()
  slicer_add_python_unittest(
    SCRIPT SequenceLightboxSelfTest.py
    SLICER_ARGS
      --additional-module-paths ${MODULE_BUILD_DIR} ${CMAKE_BINARY_DIR}/${Slicer_QTSCRIPTEDMODULES_LIB_DIR}
    )
endif()
