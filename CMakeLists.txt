# Set some CMake properties
INCLUDE( CheckCXXCompilerFlag )
INCLUDE( CheckCXXSourceCompiles )
CMAKE_MINIMUM_REQUIRED( VERSION 3.20.0 )

MESSAGE( "===========================" )
MESSAGE( "Configuring StackTrace" )
MESSAGE( "===========================" )

# Set the project name
SET( PROJ StackTrace )              # Set the project name for CMake
SET( StackTrace_INC StackTrace )    # Set the inlcude path


# Prevent users from building in place
IF ( "${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}" )
    MESSAGE( FATAL_ERROR "Building code in place is a bad idea" )
ENDIF()

# Set source/install paths
SET( ${PROJ}_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" )
SET( ${PROJ}_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
IF ( ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${${PROJ}_INSTALL_DIR}" )
ELSEIF ( NOT ${PROJ}_INSTALL_DIR )
    SET( ${PROJ}_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}" )
ENDIF()
INCLUDE_DIRECTORIES( "${${PROJ}_INSTALL_DIR}/include" )
SET( CMAKE_MODULE_PATH ${${PROJ}_SOURCE_DIR} ${${PROJ}_SOURCE_DIR}/cmake )
SET( CMAKE_INSTALL_RPATH "${${PROJ}_INSTALL_DIR}/lib" ${CMAKE_INSTALL_RPATH} )
SET( STACKTRACE_INCLUDE "${${PROJ}_INSTALL_DIR}/include/StackTrace" )

# Set the project name
PROJECT( ${PROJ} LANGUAGES CXX )

# Include CMake files
INCLUDE( cmake/macros.cmake )

# Enable testing
INCLUDE( CTest )

# Create a release target
INCLUDE( cmake/WriteRepoVersion.cmake )
WRITE_REPO_VERSION()
CREATE_RELEASE()

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
    MESSAGE( "DOXYGEN_MACROS = ${DOXYGEN_MACROS}" )
    INCLUDE( "${${PROJ}_SOURCE_DIR}/cmake/UseDoxygen.cmake" )
    IF ( DOXYGEN_FOUND )
        ADD_DEPENDENCIES( doc doxygen )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()

# Configure MPI
CHECK_ENABLE_FLAG( USE_MPI 0 )
INCLUDE( cmake/libraries.cmake )
CONFIGURE_MPI()
IF ( USE_MPI )
    SET( SET_USE_MPI "1" )
ELSE()
    SET( SET_USE_MPI "0" )
ENDIF()

# Set shared/static library
IF ( NOT DEFINED LIB_TYPE )
    IF ( ENABLE_SHARED )
        SET( LIB_TYPE SHARED )
    ELSE()
        SET( LIB_TYPE STATIC )
    ENDIF()
ENDIF()

# Configure valgrind (optional)
FIND_PACKAGE( Valgrind )
IF ( Valgrind_INCLUDE_DIR )
    INCLUDE_DIRECTORIES( "${Valgrind_INCLUDE_DIR}" )
ENDIF()

# Add system specific libraries
GET_SYSTEM_LIBS()

# Set the program to get info from addresses
SET( TPL_FILE "${CMAKE_CURRENT_BINARY_DIR}/tmp/StackTrace_TPLs.h" )
FILE( WRITE "${TPL_FILE}" "// StackTrace TPLs\n" )
IF ( LINUX )
    FIND_PROGRAM( ADDR2LINE_EXECUTABLE NAMES eu-addr2line DOC "Path to eu-addr2line" )
    IF ( NOT ADDR2LINE_EXECUTABLE )
        MESSAGE( STATUS "eu-addr2line was not found, defaulting to addr2line" )
        MESSAGE( STATUS "Note: eu-addr2line is signficantly faster for large libraries,"
                        " install elfutils for better performance" )        
        FIND_PROGRAM( ADDR2LINE_EXECUTABLE NAMES addr2line DOC "Path to addr2line utility" )
    ENDIF()
    IF ( NOT ADDR2LINE_EXECUTABLE )
        SET( ADDR2LINE_EXECUTABLE addr2line )
    ENDIF()
    FILE( APPEND "${TPL_FILE}" "#define ADDR2LINE \"${ADDR2LINE_EXECUTABLE}\"\n" )
ENDIF()

# Add distclean target
ADD_DISTCLEAN(
    a.out
    libstacktrace*
    FindStackTrace.cmake
    TestStack
    TestTerminate
    TestUtilities
    ExampleStack.txt
    cppcheck-build
    test_mpi.cpp
    test_source_location.cpp
    STACKTRACE-*.*.*.tar.gz )

# Check if we want to enable coverage
IF ( ENABLE_GCOV OR ENABLE_COVERAGE )
    FIND_PACKAGE( Coverage )
ENDIF()
IF ( COVERAGE_FOUND )
    SET( ENABLE_COVERAGE true )
    SET( COVERAGE_FLAGS ${COVERAGE_FLAGS} )
    SET( COVERAGE_LIBS ${COVERAGE_LIBS} )
ENDIF()

# Set compiler flags
SET_COMPILER_STD()
SET_COMPILER_FLAGS()

# Check if we are using the timer (allows a more accurate memory usage)
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
    FIND_PACKAGE( Timer REQUIRED PATHS "${TIMER_DIRECTORY}/lib/cmake" )
    SET( TIMER_LIB Timer::timerutility )
ENDIF()

# Create a target to copy headers
ADD_CUSTOM_TARGET( StackTrace-include ALL )
FILE( GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" )
FILE( GLOB hfiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${headers} )
FOREACH( tmp ${hfiles} )
    SET( SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${tmp}" )
    SET( DST_FILE "${STACKTRACE_INCLUDE}/${tmp}" )
    CONFIGURE_FILE( "${SRC_FILE}" "${DST_FILE}" @ONLY )
ENDFOREACH()
IF ( "${CMAKE_CXX_STANDARD}" STREQUAL "17" )
    CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/source_location.c17"
                    "${STACKTRACE_INCLUDE}/source_location.h" @ONLY )
ELSE()
    CONFIGURE_FILE( "${CMAKE_CURRENT_SOURCE_DIR}/source_location.c20"
                    "${STACKTRACE_INCLUDE}/source_location.h" @ONLY )
ENDIF()

# Check for mallinfo2
INCLUDE( CheckSymbolExists )
CHECK_SYMBOL_EXISTS( mallinfo2 malloc.h HAVE_MALLINFO2 )
IF ( HAVE_MALLINFO2 )
    ADD_DEFINITIONS( -DHAVE_MALLINFO2 )
ENDIF()

# Add library
ADD_LIBRARY( stacktrace ${LIB_TYPE} Utilities.cpp StackTrace.cpp StackTraceThreads.cpp )
ADD_DEPENDENCIES( stacktrace StackTrace-include )
TARGET_LINK_LIBRARIES( stacktrace ${CMAKE_DL_LIBS} ${SYSTEM_LIBS} ${TIMER_LIB} ${MPICXX_LIBS} )
INSTALL( TARGETS stacktrace DESTINATION "${${PROJ}_INSTALL_DIR}/lib" )
INSTALL( TARGETS stacktrace EXPORT StackTraceTargets DESTINATION "${${PROJ}_INSTALL_DIR}/lib" )
INSTALL( EXPORT StackTraceTargets FILE StackTraceTargets.cmake NAMESPACE StackTrace:: DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace )
TARGET_COMPILE_DEFINITIONS( stacktrace PUBLIC ${COVERAGE_FLAGS} )

# Generate a Package Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR "${${PROJ}_INSTALL_DIR}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR "${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace" CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/StackTrace.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceConfig.cmake" INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace"
                              PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE( "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceVersion.cmake" VERSION ${${PROJ}_VERSION} COMPATIBILITY SameMajorVersion )
INSTALL( FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceVersion.cmake"
        DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace )
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TPL_FILE}" "${${PROJ}_INSTALL_DIR}/include/StackTrace/StackTrace_TPLs.h" )

# Add the tests
IF ( BUILD_TESTING )
    ADD_EXE( TestStack TestStack.cpp )
    ADD_EXE( TestTerminate TestTerminate.cpp )
    ADD_EXE( TestUtilities TestUtilities.cpp )
    CONFIGURE_FILE( "data/ExampleStack.txt" "${CMAKE_CURRENT_BINARY_DIR}/ExampleStack.txt" @ONLY )
    CONFIGURE_FILE( "data/ExampleStack.txt" "${${PROJ}_INSTALL_DIR}/bin/ExampleStack.txt" @ONLY )
    ADD_TEST( NAME TestStack COMMAND $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestUtilities COMMAND $<TARGET_FILE:TestUtilities> )
    IF ( USE_MPI AND DEFINED MPIEXEC )
        ADD_TEST( NAME TestStack-4procs COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
    ENDIF()
ENDIF()

# Add the cppcheck tests
SET( CPPCHECK_USE_JSON TRUE )
FIND_PACKAGE( Cppcheck )

# Add the cppclean tests
SET( CPPCLEAN_OPTIONS )
SET( CPPCLEAN_EXCLUDE )
SET( CPPCLEAN_SUPPRESSIONS )
SET( CPPCLEAN_FAIL_ON_WARNINGS 1 )
SET( CPPCLEAN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
FIND_PACKAGE( Cppclean )
