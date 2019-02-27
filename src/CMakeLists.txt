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


# Inlcude cmake files
INCLUDE( cmake/macros.cmake )


# Enable testing
ENABLE_TESTING()
INCLUDE( CTest )


# Add system specific libraries
GET_SYSTEM_LIBS()


# Add distclean target
ADD_DISTCLEAN( libstacktrace* FindStackTrace.cmake TestStack )


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


# Set compiler flags
IF ( NOT CMAKE_CXX_STANDARD )
    SET( CMAKE_CXX_STANDARD 14 )
ENDIF()
SET_COMPILER_FLAGS()


# Check if we are using the timer (allows a more accurace memory usage)
IF ( TIMER_DIRECTORY )
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


# Add an executable
ADD_EXECUTABLE( TestStack TestStack.cpp )
TARGET_LINK_LIBRARIES( TestStack stacktrace )
TARGET_LINK_LIBRARIES( TestStack ${TIMER_LIB} ${MPI_CXX_LIBRARIES} "${SYSTEM_LIBS}" )
INSTALL( TARGETS TestStack DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" )
CONFIGURE_FILE( "data/ExampleStack.txt" "${CMAKE_CURRENT_BINARY_DIR}/ExampleStack.txt" @ONLY )
IF ( USE_MPI )
    ADD_TEST( NAME TestStack COMMAND ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 4 $<TARGET_FILE:TestStack> )
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

