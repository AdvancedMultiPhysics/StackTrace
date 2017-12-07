# FindStackTrace
# ---------
#
# Find the FindStackTrace package
#
# Use this module by invoking find_package with the form:
#
#   find_package( StackTrace
#     [version] [EXACT]         # Minimum or EXACT version e.g. 1.36.0
#     [REQUIRED]                # Fail with error if the TPLs are not found
#     [COMPONENTS <libs>...]    # List of TPLs to include
#   )
#
# This module finds headers and requested component libraries for the StackTrace library
#
#   StackTrace_FOUND           - True if headers and requested libraries were found
#   StackTrace_LIBRARIES       - Libraries (and dependencies)
#   StackTrace_INCLUDE_DIRS    - Include paths


# Add the libraries for the atomic model
SET( StackTrace_FOUND TRUE )
SET( CMAKE_INSTALL_RPATH "@CMAKE_INSTALL_PREFIX@/lib" @CMAKE_INSTALL_RPATH@ ${CMAKE_INSTALL_RPATH} )
FIND_LIBRARY( StackTrace_LIB  NAMES stacktrace  PATHS "@CMAKE_INSTALL_PREFIX@/lib" NO_DEFAULT_PATH )
SET( StackTrace_LIBRARIES ${StackTrace_LIB} @SYSTEM_LIBS@ )
SET( StackTrace_INCLUDE_DIRS "@CMAKE_INSTALL_PREFIX@/include" )

