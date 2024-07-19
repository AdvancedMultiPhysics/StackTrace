# Set some CMake properties
INCLUDE(CheckCXXCompilerFlag)
INCLUDE(CheckCXXSourceCompiles)
CMAKE_MINIMUM_REQUIRED(VERSION 3.9)


MESSAGE( "===========================" )
MESSAGE( "Configuring StackTrace"      )
MESSAGE( "===========================" )


# Set the project name
SET( PROJ StackTrace )              # Set the project name for CMake


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
SET( CMAKE_INSTALL_RPATH "${${PROJ}_INSTALL_DIR}/lib" ${CMAKE_INSTALL_RPATH} )
SET( STACKTRACE_INCLUDE "${${PROJ}_INSTALL_DIR}/include/StackTrace" )


# Set the project name
PROJECT( ${PROJ} LANGUAGES CXX )


# Include CMake files
INCLUDE( cmake/macros.cmake )


# Enable testing
ENABLE_TESTING()
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
    MESSAGE("DOXYGEN_MACROS = ${DOXYGEN_MACROS}")
    INCLUDE( "${${PROJ}_SOURCE_DIR}/cmake/UseDoxygen.cmake" )
    IF ( DOXYGEN_FOUND )
        ADD_DEPENDENCIES( doc doxygen )
    ELSE()
        SET( USE_DOXYGEN 0 )
    ENDIF()
ENDIF()


# Configure MPI
IF ( USE_MPI )
    MESSAGE( "Configuring MPI" )
    IF ( MPIEXEC )
        SET( MPIEXEC_EXECUTABLE ${MPIEXEC} )
    ENDIF()
    IF ( NOT MPI_SKIP_SEARCH )
        FIND_PACKAGE( MPI )
    ELSE()
        # Write mpi test
        SET( MPI_TEST_SRC "${CMAKE_CURRENT_BINARY_DIR}/test_mpi.cpp" )
        FILE(WRITE  ${MPI_TEST_SRC} "#include <mpi.h>\n" )
        FILE(APPEND ${MPI_TEST_SRC} "int main(int argc, char** argv) {\n" )
        FILE(APPEND ${MPI_TEST_SRC} "    MPI_Init(NULL, NULL);\n")
        FILE(APPEND ${MPI_TEST_SRC} "    MPI_Finalize();\n" )
        FILE(APPEND ${MPI_TEST_SRC} "}\n" )
        # Test the compile
        SET( TMP_FLAGS -DINCLUDE_DIRECTORIES=${MPI_CXX_INCLUDE_PATH} )
        TRY_COMPILE( MPI_TEST_CXX ${CMAKE_CURRENT_BINARY_DIR} ${MPI_TEST_SRC}
            CMAKE_FLAGS ${TMP_FLAGS}
            LINK_OPTIONS ${MPI_CXX_LINK_FLAGS}
            LINK_LIBRARIES ${MPI_CXX_LIBRARIES}
            OUTPUT_VARIABLE OUT_TXT)
        IF ( NOT ${MPI_TEST} )
            MESSAGE( FATAL_ERROR "Skipping MPI search and default compile fails:\n${OUT_TXT}" )
        ENDIF()
        SET( MPI_CXX_FOUND TRUE )
    ENDIF()
    STRING( STRIP "${MPI_CXX_COMPILE_FLAGS}" MPI_CXX_COMPILE_FLAGS )
    STRING( STRIP "${MPI_CXX_LINK_FLAGS}" MPI_CXX_LINK_FLAGS )
    STRING( STRIP "${MPI_CXX_LIBRARIES}" MPI_CXX_LIBRARIES )
    MESSAGE( "   MPI_CXX_FOUND = ${MPI_CXX_FOUND}" )
    MESSAGE( "   MPI_CXX_COMPILER = ${MPI_CXX_COMPILER}" )
    MESSAGE( "   MPI_CXX_COMPILE_FLAGS = ${MPI_CXX_COMPILE_FLAGS}" )
    MESSAGE( "   MPI_CXX_INCLUDE_PATH = ${MPI_CXX_INCLUDE_PATH}" )
    MESSAGE( "   MPI_CXX_LINK_FLAGS = ${MPI_CXX_LINK_FLAGS}" )
    MESSAGE( "   MPI_CXX_LIBRARIES = ${MPI_CXX_LIBRARIES}" )
    MESSAGE( "   MPIEXEC = ${MPIEXEC}" )
    MESSAGE( "   MPIEXEC_NUMPROC_FLAG = ${MPIEXEC_NUMPROC_FLAG}" )
    MESSAGE( "   MPIEXEC_PREFLAGS = ${MPIEXEC_PREFLAGS}" )
    MESSAGE( "   MPIEXEC_POSTFLAGS = ${MPIEXEC_POSTFLAGS}" )
    IF ( NOT MPI_CXX_FOUND )
        MESSAGE( FATAL_ERROR "MPI not found" )
    ENDIF()
    ADD_DEFINITIONS( -DUSE_MPI )
    INCLUDE_DIRECTORIES( ${MPI_CXX_INCLUDE_PATH} )
    SET( SET_USE_MPI "1" )
ELSE()
    SET( SET_USE_MPI "0" )
ENDIF()


# Add system specific libraries
GET_SYSTEM_LIBS()


# Add distclean target
ADD_DISTCLEAN( libstacktrace* FindStackTrace.cmake TestStack TestTerminate TestUtilities ExampleStack.txt cppcheck-build STACKTRACE-*.*.*.tar.gz )


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
    SET( CMAKE_CXX_STANDARD 17 )
ENDIF()
IF ( "${CMAKE_CXX_STANDARD}" LESS "17" )
    MESSAGE( FATAL_ERROR "C++17 or newer required" )
ELSE()
    FILE( WRITE "${STACKTRACE_INCLUDE}/string_view.h" "namespace StackTrace {\nusing std::string_view;\n}\n" )
ENDIF()
FILE( WRITE "${STACKTRACE_INCLUDE}/std.h" "#define STACTRACE_CXX_STANDARD ${CMAKE_CXX_STANDARD}\n" )
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


# Check for mallinfo2
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS( mallinfo2 malloc.h HAVE_MALLINFO2 )
IF ( HAVE_MALLINFO2 )
    ADD_DEFINITIONS( -DHAVE_MALLINFO2 )
ENDIF()


# Add a static library
ADD_LIBRARY( stacktrace ${LIB_TYPE} Utilities.cpp StackTrace.cpp StackTraceThreads.cpp )
ADD_DEPENDENCIES( stacktrace StackTrace-include )
TARGET_LINK_LIBRARIES( stacktrace ${CMAKE_DL_LIBS} ${TIMER_LIB} )
INSTALL( TARGETS stacktrace DESTINATION "${${PROJ}_INSTALL_DIR}/lib" )
INSTALL( TARGETS stacktrace EXPORT StackTraceTargets DESTINATION "${${PROJ}_INSTALL_DIR}/lib" )
INSTALL( EXPORT StackTraceTargets
         FILE StackTraceTargets.cmake
         NAMESPACE StackTrace::
         DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace
)
TARGET_COMPILE_DEFINITIONS( stacktrace PUBLIC ${COVERAGE_FLAGS} )


# Generate a Package Configuration File
INCLUDE( CMakePackageConfigHelpers )
SET( INCLUDE_INSTALL_DIR  "${${PROJ}_INSTALL_DIR}/include" CACHE PATH "Location of header files" )
SET( SYSCONFIG_INSTALL_DIR "${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace" CACHE PATH "Location of configuration files" )
CONFIGURE_PACKAGE_CONFIG_FILE( cmake/StackTrace.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceConfig.cmake"
    INSTALL_DESTINATION "${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace"
    PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR )
WRITE_BASIC_PACKAGE_VERSION_FILE(
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceVersion.cmake"
    VERSION ${${PROJ}_VERSION}
    COMPATIBILITY SameMajorVersion )
INSTALL( FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceConfig.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake/StackTraceVersion.cmake"
         DESTINATION ${${PROJ}_INSTALL_DIR}/lib/cmake/StackTrace )


# Add the tests
ADD_EXE( TestStack TestStack.cpp )
ADD_EXE( TestTerminate TestTerminate.cpp )
ADD_EXE( TestUtilities TestUtilities.cpp )
CONFIGURE_FILE( "data/ExampleStack.txt" "${CMAKE_CURRENT_BINARY_DIR}/ExampleStack.txt" @ONLY )
CONFIGURE_FILE( "data/ExampleStack.txt" "${${PROJ}_INSTALL_DIR}/bin/ExampleStack.txt" @ONLY )
IF ( USE_MPI )
    ADD_TEST( NAME TestStack COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestStack-4procs COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestUtilities COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 1 $<TARGET_FILE:TestUtilities> )
    ADD_TEST( NAME TestUtilities-4procs COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestUtilities> )
ELSE()
    ADD_TEST( NAME TestStack COMMAND $<TARGET_FILE:TestStack> )
    ADD_TEST( NAME TestUtilities COMMAND $<TARGET_FILE:TestUtilities> )
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

