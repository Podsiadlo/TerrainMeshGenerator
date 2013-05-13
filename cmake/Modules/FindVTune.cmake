# Find VTune libraries
# Once done this will define
#  VTune_FOUND - System has VTune
#  VTune_INCLUDE_DIRS - The VTune include directories
#  VTune_LIBRARIES - The libraries needed to use VTune
set(VTune_DIR "/opt/intel/vtune_amplifier_xe_2013;/opt/intel/vtune_amplifier_xe_2011")

if(VTune_INCLUDE_DIRS AND VTune_LIBRARIES)
  set(VTune_FIND_QUIETLY TRUE)
endif()

find_path(VTune_INCLUDE_DIRS ittnotify.h PATHS ${VTune_DIR} PATH_SUFFIXES include)
find_library(VTune_LIBRARIES NAMES ittnotify PATHS ${VTune_DIR} PATH_SUFFIXES lib lib64 lib32)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VTune DEFAULT_MSG VTune_LIBRARIES VTune_INCLUDE_DIRS)
if(VTUNE_FOUND)
  set(VTune_FOUND on)
endif()
mark_as_advanced(VTune_INCLUDE_DIRS VTune_LIBRARIES)
