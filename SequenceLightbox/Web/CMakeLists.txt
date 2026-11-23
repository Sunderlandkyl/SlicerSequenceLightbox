#-----------------------------------------------------------------------------
# The module in the web application of Slicer (SlicerWeb), which builds none of the Qt classes of
# the module: a scripted module that does what they do (see SequenceLightboxWeb.py).
set(MODULE_NAME SequenceLightboxWeb)

set(MODULE_PYTHON_SCRIPTS
  ${MODULE_NAME}.py
  )

slicerMacroBuildScriptedModule(
  NAME ${MODULE_NAME}
  SCRIPTS ${MODULE_PYTHON_SCRIPTS}
  )
