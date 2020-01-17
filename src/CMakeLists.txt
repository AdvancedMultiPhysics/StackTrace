# Set some CMake properties
INCLUDE(CheckCXXCompilerFlag)
INCLUDE(CheckCXXSourceCompiles)
CMAKE_MINIMUM_REQUIRED(VERSION 3.9)


MESSAGE( "===========================" )
MESSAGE( "Configuring StackTrace"      )
MESSAGE( "===========================" )


# Set the project name
SET( PROJ STACKTRACE )              # Set the project name for CMake


# Prevent users from building in place
IF ("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()


# Set source/install paths
SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( ${PROJ}_BUILD_DIR  "${CMAKE_CURRENT_BINARY_DIR}" )
IF( ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${${PROJ}_INSTALL_DIR}" )
ELSEIF( NOT ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
INCLUDE_DIRECTORIES( "${${PROJ}_INSTALL_DIR}/include" )
SET( CMAKE_MODULE_PATH ${${PROJ}_SOURCE_DIR} ${${PROJ}_SOURCE_DIR}/cmake )


# Set the project name
PROJECT( ${PROJ} LANGUAGES CXX )


# Include CMake files
INCLUDE( cmake/macros.cmake )


# Enable testing
ENABLE_TESTING()
INCLUDE( CTest )

# Create the target for documentation
IF ( NOT DEFINED USE_DOXYGEN )
    SET( USE_DOXYGEN 1 )
ENDIF()
IF ( USE_DOXYGEN )
    ADD_CUSTOM_TARGET( doc )
    FILE( MAKE_DIRECTORY "${${PROJ}_INSTALL_DIR}/doc" )
    SET( DOXYFILE_LATEX "YES" )
    SET( DOXYFILE_IN "${${PROJ}_SOURCE_DIR}/doxygen/Doxyfile.in" )
    SET( DOXY_HEADER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/header.html" )
    SET( DOXY_FOOTER_FILE "${${PROJ}_SOURCE_DIR}/doxygen/html/footer.html" )
    SET( DOXYFILE_OUTPUT_DIR "${${PROJ}_INSTALL_DIR}/doc" )
    SET( DOXYFILE_SRC_HTML_DIR "${${PROJ}_SOURCE_DIR}/doxygen/html" )
    SET( DOXYFILE_SOURCE_DIR "${${PROJ}_SOURCE_DIR}" )
    SET( REL_PACKAGE_HTML "" )
    SET( DOXYGEN_MACROS "" )
    MESSAGE("DOXYGEN_MACROS = ${DOXYGEN_MACROS}")
    INCLUDE( "${${PROJ}_SOURCE_DIR}/cmake/UseDoxygen.cmake" )
    IF ( DOXYGEN_FOUND )
        ADD_DEPENDENCIES( doc doxygen )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()


# Add system specific libraries
GET_SYSTEM_LIBS()


# Add distclean target
ADD_DISTCLEAN( libstacktrace* FindStackTrace.cmake TestStack TestTerminate ExampleStack.txt )


# Set the install path
IF ( INSTALL_PREFIX )
    SET( CMAKE_INSTALL_PREFIX "${INSTALL_PREFIX}" )
ELSE()
    SET( CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
SET( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH} )


# Add the include directory
INCLUDE_DIRECTORIES( "${CMAKE_INSTALL_PREFIX}/include" )
IF ( USE_MPI )
    FIND_PACKAGE( MPI )
    ADD_DEFINITIONS( -DUSE_MPI )
    INCLUDE_DIRECTORIES( ${MPI_CXX_INCLUDE_PATH} )
    SET( MPI_COMM "#include \"mpi.h\"" )
ELSE()
    SET( MPI_COMM "typedef int MPI_Comm;" )
ENDIF()


# Check if we want to enable coverage
IF ( ENABLE_GCOV OR ENABLE_COVERAGE )
    FIND_PACKAGE( Coverage )
ENDIF()
IF ( COVERAGE_FOUND )
    SET( ENABLE_COVERAGE true )
    SET( COVERAGE_FLAGS ${COVERAGE_FLAGS} )
    SET( COVERAGE_LIBS  ${COVERAGE_LIBS} )
ENDIF()


# Set compiler flags
IF ( NOT CMAKE_CXX_STANDARD )
    SET( CMAKE_CXX_STANDARD 14 )
ENDIF()
SET_COMPILER_FLAGS()


# Check if we are using the timer (allows a more accurace memory usage)
IF ( NOT DEFINED USE_TIMER )
    IF ( TIMER_DIRECTORY )
        SET( USE_TIMER 1 )
    ELSE()
        SET( USE_TIMER 0 )
    ENDIF()
ENDIF()
IF ( USE_TIMER )
    ADD_DEFINITIONS( -DUSE_TIMER )
    INCLUDE_DIRECTORIES( "${TIMER_DIRECTORY}/include" )
    FIND_LIBRARY( TIMER_LIB timerutility PATHS "${TIMER_DIRECTORY}/lib" NO_DEFAULT_PATH )
ENDIF()
SET_COMPILER_FLAGS()


# Create a target to copy headers
ADD_CUSTOM_TARGET( StackTrace-include ALL )
FILE(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
FILE( GLOB hfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${headers} )
FOREACH( tmp ${hfiles} )
    SET( SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${tmp}" )
    SET( DST_FILE "${CMAKE_INSTALL_PREFIX}/include/StackTrace/${tmp}" )
    CONFIGURE_FILE( "${SRC_FILE}" "${DST_FILE}" @ONLY )
ENDFOREACH()


# Add a static library
ADD_LIBRARY( stacktrace STATIC Utilities.cpp StackTrace.cpp )
ADD_DEPENDENCIES( stacktrace StackTrace-include )
INSTALL( TARGETS stacktrace DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" )
TARGET_COMPILE_DEFINITIONS( stacktrace PUBLIC ${COVERAGE_FLAGS} )


# Add the tests
ADD_EXE( TestStack TestStack.cpp )
ADD_EXE( TestTerminate TestTerminate.cpp )
CONFIGURE_FILE( "data/ExampleStack.txt" "${CMAKE_CURRENT_BINARY_DIR}/ExampleStack.txt" @ONLY )
CONFIGURE_FILE( "data/ExampleStack.txt" "${CMAKE_INSTALL_PREFIX}/bin/ExampleStack.txt" @ONLY )
IF ( USE_MPI )
    ADD_TEST( NAME TestStack COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestStack-4procs COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
ELSE()
    ADD_TEST( NAME TestStack COMMAND $<TARGET_FILE:TestStack> )
ENDIF()


# Create FindStackTrace.cmake
CONFIGURE_FILE( cmake/FindStackTrace.template.cmake "${CMAKE_INSTALL_PREFIX}/FindStackTrace.cmake" @ONLY )


# Add the cppcheck tests
SET( CPPCHECK_INCLUDE "-I${CMAKE_INSTALL_PREFIX}/include" )
SET( CPPCHECK_SERIALIZE TRUE )
SET( CPPCHECK_SUPRESSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cppcheckSuppressionFile" )
FIND_PACKAGE( Cppcheck )


# Add the cppclean tests
SET( CPPCLEAN_OPTIONS )
SET( CPPCLEAN_EXCLUDE )
SET( CPPCLEAN_SUPPRESSIONS )
SET( CPPCLEAN_FAIL_ON_WARNINGS 1 )
SET( CPPCLEAN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
FIND_PACKAGE( Cppclean )

