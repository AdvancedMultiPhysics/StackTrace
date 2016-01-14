INCLUDE(CheckCXXCompilerFlag)
INCLUDE(CheckCXXSourceCompiles)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

MESSAGE( "===========================" )
MESSAGE( "Configuring StackTrace"      )
MESSAGE( "===========================" )


# Prevent users from building in place
IF ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()


# Set the project name
PROJECT( STACKTRACE )


# Add the include directory
INCLUDE( "${CMAKE_CURRENT_SOURCE_DIR}" )
ADD_DEFINITIONS( -D USE_MPI )


# Try to add rdynamic so we have names in backtrace
SET( CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS} -ldl" )
CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" ldl )
IF ( ldl )
    SET( SYSTEM_LDFLAGS ${SYSTEM_LDFLAGS} -ldl )
ENDIF()


# Add distclean target
ADD_CUSTOM_TARGET (distclean @echo cleaning for source distribution)
IF (UNIX)
    ADD_CUSTOM_COMMAND( DEPENDS clean  COMMENT "distribution clean"
        COMMAND rm
        ARGS -Rf CMake* libstacktrace.a Makefile cmake_install.cmake install_manifest.txt lib TestStack
        TARGET  distclean
    )
ENDIF()


# Add a static library
ADD_LIBRARY( stacktrace STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Utilities.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StackTrace.cpp" )
INSTALL( TARGETS stacktrace DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/lib" )


# Add an executable
ADD_EXECUTABLE( TestStack "${CMAKE_CURRENT_SOURCE_DIR}/TestStack.cpp" )
TARGET_LINK_LIBRARIES( TestStack stacktrace ${SYSTEM_LDFLAGS} )



