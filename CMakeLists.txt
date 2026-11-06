cmake_minimum_required(VERSION 3.16.3...3.28.0 FATAL_ERROR)

project(SlicerSequenceLightbox)

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Sunderlandkyl/SlicerSequenceLightbox")
set(EXTENSION_CATEGORY "Sequences")
set(EXTENSION_CONTRIBUTORS "Kyle Sunderland (Radixlab Medical Inc.)")
set(EXTENSION_DESCRIPTION "Lightbox and timeline views that display several items of a sequence side by side. The tiles follow a slice view or a 3D view and can be interacted with like regular views.")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Sunderlandkyl/SlicerSequenceLightbox/main/SlicerSequenceLightbox.png")
set(EXTENSION_SCREENSHOTURLS "")
set(EXTENSION_DEPENDS "NA") # Specified as a list or "NA" if no dependencies

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(SequenceLightbox)
## NEXT_MODULE

#-----------------------------------------------------------------------------
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
