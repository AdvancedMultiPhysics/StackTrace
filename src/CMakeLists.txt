INCLUDE(CheckCXXCompilerFlag)
INCLUDE(CheckCXXSourceCompiles)
CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

MESSAGE( "===========================" )
MESSAGE( "Configuring StackTrace"      )
MESSAGE( "===========================" )


# Prevent users from building in place
IF ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()


# Set the project name
PROJECT( STACKTRACE LANGUAGES CXX )


# Enable testing
ENABLE_TESTING()
INCLUDE( CTest )


# Add system specific libraries
IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
    # Windows specific system libraries
    SET( SYSTEM_PATHS "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/x64" 
                      "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/PlatformSDK/Lib/AMD64" 
                      "C:/Program Files (x86)/Microsoft Visual Studio 12.0/Common7/Packages/Debugger/X64" )
    FIND_LIBRARY( PSAPI_LIB    NAMES Psapi    PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
    FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp  PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
    FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp )
    IF ( PSAPI_LIB ) 
        ADD_DEFINITIONS( -D PSAPI )
        SET( SYSTEM_LIBS ${PSAPI_LIB} )
    ENDIF()
    IF ( DBGHELP_LIB ) 
        ADD_DEFINITIONS( -D DBGHELP )
        SET( SYSTEM_LIBS ${DBGHELP_LIB} )
    ELSE()
        MESSAGE( WARNING "Did not find DbgHelp, stack trace will not be availible" )
    ENDIF()
    MESSAGE("System libs: ${SYSTEM_LIBS}")
ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    # Linux specific system libraries
    SET( SYSTEM_LIBS "-ldl -lpthread" )
    IF ( NOT USE_STATIC )
        SET( SYSTEM_LIBS "${SYSTEM_LIBS} -rdynamic" )   # Needed for backtrace to print function names
    ENDIF()
ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" )
    # Max specific system libraries
    SET( SYSTEM_LIBS "-ldl -lpthread" )
ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Generic" )
    # Generic system libraries
ELSE()
    MESSAGE( FATAL_ERROR "OS not detected" )
ENDIF()


# Try to add Dbghelp.lib
#SET( CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} Dbghelp.lib" )
#CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" Dbghelp )
#IF ( Dbghelp )
    SET( SYSTEM_LDFLAGS "${SYSTEM_LDFLAGS} Dbghelp.lib" )
#ENDIF()


# Add distclean target
ADD_CUSTOM_TARGET (distclean @echo cleaning for source distribution)
IF (UNIX)
    ADD_CUSTOM_COMMAND( DEPENDS clean  COMMENT "distribution clean"
        COMMAND rm
        ARGS -Rf CMake* libstacktrace.a Makefile cmake_install.cmake install_manifest.txt
                 lib include TestStack CTestTestfile.cmake  DartConfiguration.tcl  Testing
        TARGET  distclean
    )
ENDIF()


# Set the install path
IF ( INSTALL_PREFIX )
    SET( CMAKE_INSTALL_PREFIX "${INSTALL_PREFIX}" )
ELSE()
    SET( CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()


# Add the include directory
INCLUDE_DIRECTORIES( "${CMAKE_CURRENT_SOURCE_DIR}" )
ADD_DEFINITIONS( -DDBGHELP )
IF ( USE_MPI )
    FIND_PACKAGE( MPI )
    ADD_DEFINITIONS( -DUSE_MPI )
ENDIF()


# Add extra compile flags
SET( CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} --std=c++11 )


# Add a static library
ADD_LIBRARY( stacktrace STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Utilities.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StackTrace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnitTest.cpp" )
INSTALL( TARGETS stacktrace DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" )
FILE(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
INSTALL(FILES ${headers} DESTINATION "${CMAKE_INSTALL_PREFIX}/include" )

# Add an executable
ADD_EXECUTABLE( TestStack "${CMAKE_CURRENT_SOURCE_DIR}/TestStack.cpp" )
TARGET_LINK_LIBRARIES( TestStack stacktrace )
TARGET_LINK_LIBRARIES( TestStack "${SYSTEM_LIBS}" )

IF ( USE_MPI )
    ADD_TEST( NAME TestStack COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestStack-4procs COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
ELSE()
    ADD_TEST( NAME TestStack COMMAND $<TARGET_FILE:TestStack> )
ENDIF()

