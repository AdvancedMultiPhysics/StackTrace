# FindCppcheck
# ---------
#
# Find cppcheck
#
# Use this module by invoking find_package with the form:
#
#   find_package( Cppcheck
#     [REQUIRED]             # Fail with error if the cppcheck is not found
#   )
#
# This module finds cppcheck and configures a test using the provided options
#
# This program reconizes the following options
#   CPPCHECK_INCLUDE      - List of include folders
#   CPPCHECK_OPTIONS      - List of cppcheck options
#   CPPCHECK_SOURCE       - Source path to check
#   CPPCHECK_TIMEOUT      - Timeout for each cppcheck test (default is 5 minutes)
#
# The following variables are set by find_package( Cppcheck )
#
#   CPPCHECK_FOUND        - True if cppcheck was found


# Find cppcheck if availible
FIND_PROGRAM( CPPCHECK 
    NAMES cppcheck cppcheck.exe 
    PATHS "${CPPCHECK_DIRECTORY}" "${CPPCHECK_DIRECTORY}/bin" NO_DEFAULT_PATH
)
FIND_PROGRAM( CPPCHECK 
    NAMES cppcheck cppcheck.exe 
    PATHS "C:/Program Files/Cppcheck" "C:/Program Files (x86)/Cppcheck" NO_DEFAULT_PATH
)
FIND_PROGRAM( CPPCHECK 
    NAMES cppcheck cppcheck.exe 
)
IF ( CPPCHECK )
    SET( CPPCHECK_FOUND TRUE )
ELSE()
    SET( CPPCHECK_FOUND FALSE )
ENDIF()
IF ( CPPCHECK_FOUND )
    EXECUTE_PROCESS( COMMAND ${CPPCHECK} --version OUTPUT_VARIABLE CPPCHECK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE )
    MESSAGE( STATUS "Using cppcheck: ${CPPCHECK_VERSION}")
ELSEIF ( CPPCHECK_FIND_REQUIRED )
    MESSAGE( FATAL_ERROR "cppcheck not found")
ELSE()
    MESSAGE( STATUS "cppcheck not found")
ENDIF()


# Set the source directory (if not set)
IF ( NOT DEFINED CPPCHECK_SOURCE )
    IF ( DEFINED ${PROJ}_SOURCE_DIR )
        SET( CPPCHECK_SOURCE "${${PROJ}_SOURCE_DIR}" )
    ELSE()
        SET( CPPCHECK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
    ENDIF()
ENDIF()


# Function to add a cppcheck tests
FUNCTION( ADD_CPPCHECK_TEST TESTNAME SRCDIR )
    # Check if SRCDIR has been processed by CMake
    STRING(REGEX REPLACE "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}" BINDIR "${SRCDIR}" )
    SET( PROCESSED FALSE )
    IF ( EXISTS "${BINDIR}" )
        SET( PROCESSED TRUE )
    ENDIF()

    # Set the options for cppcheck
    IF ( NOT DEFINED CPPCHECK_OPTIONS )
        SET( CPPCHECK_OPTIONS -q --enable=all --suppress=missingIncludeSystem )
        IF ( CPPCHECK_SUPRESSION_FILE )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${CPPCHECK_SUPRESSION_FILE}" )
        ELSEIF ( EXISTS "${PROJECT_SOURCE_DIR}/cppcheckSuppressionFile" )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${PROJECT_SOURCE_DIR}/cppcheckSuppressionFile" )
        ENDIF()
        IF ( CMAKE_C_STANDARD STREQUAL 99 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c99 )
        ELSEIF ( CMAKE_C_STANDARD STREQUAL 11 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c11 )
        ENDIF()
        IF ( CMAKE_CXX_STANDARD STREQUAL 98 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++0 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 11 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++11 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 14 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++14 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 14 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++17 )
        ENDIF()
        # Set definitions
        IF ( PROCESSED )
            GET_DIRECTORY_PROPERTY( DirDefs DIRECTORY "${SRCDIR}" COMPILE_DEFINITIONS )
        ELSE()
            GET_DIRECTORY_PROPERTY( DirDefs DIRECTORY "${PROJECT_SOURCE_DIR}" COMPILE_DEFINITIONS )
        ENDIF()
        FOREACH( def ${DirDefs} )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} -D${def} )
        ENDFOREACH()
        # Set OS specific defines
        IF ( WIN32 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} -DWIN32 -DWIN64 -D_WIN32 -D_WIN64 -U__APPLE__ -U__linux -U__unix -U__posix )
        ELSEIF( APPLE )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} -UWIN32 -UWIN64 -U_WIN32 -U_WIN64 -D__APPLE__ -U__linux -U__unix -U__posix )
        ELSEIF( UNIX )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} -UWIN32 -UWIN64 -U_WIN32 -U_WIN64 -U__APPLE__ -D__linux -D__unix -D__posix )
        ENDIF()
    ENDIF()
    # Add the include paths
    IF( NOT DEFINED CPPCHECK_INCLUDE )
        SET( CPPCHECK_INCLUDE )
        IF ( PROCESSED )
            GET_PROPERTY( dirs DIRECTORY "${SRCDIR}" PROPERTY INCLUDE_DIRECTORIES )
        ELSE()
            GET_PROPERTY( dirs DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES )
        ENDIF()
        LIST( REMOVE_DUPLICATES dirs )
        FOREACH(dir ${dirs})
            SET( CPPCHECK_INCLUDE ${CPPCHECK_INCLUDE} "-I${dir}" )
        ENDFOREACH()
    ENDIF()

    # Set the timeout
    IF ( NOT DEFINED CPPCHECK_TIMEOUT )
        SET( CPPCHECK_TIMEOUT 300 )
    ENDIF()

    # Add the test
    ADD_TEST( ${TESTNAME} ${CPPCHECK} ${CPPCHECK_OPTIONS} --error-exitcode=1  ${CPPCHECK_INCLUDE} ${ARGN} )
    SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES PROCESSORS 1 TIMEOUT ${CPPCHECK_TIMEOUT} COST ${CPPCHECK_TIMEOUT} )

ENDFUNCTION()


# Add the cppcheck test splitting directories with too many files
FUNCTION( ADD_CPPCHECK_TEST_RECURSIVE TESTNAME SRCDIR )
    LIST(LENGTH SRCDIR src_len)
    IF ( src_len GREATER 1 )
        # Multiple src directories
        FOREACH(src ${SRCDIR})
            FILE(GLOB child RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${src}" )
            ADD_CPPCHECK_TEST_RECURSIVE( ${TESTNAME}-${child} "${src}" )
        ENDFOREACH()
    ELSE()
        # Find the number of files to determine if we want one (or multiple) cppcheck commands
        FILE(GLOB_RECURSE SRCS "${SRCDIR}/*.cpp" "${SRCDIR}/*.cc" "${SRCDIR}/*.c" )
        LIST(LENGTH SRCS len)
        IF ( len LESS 100 OR CPPCHECK_SERIALIZE )
            ADD_CPPCHECK_TEST( ${TESTNAME} "${SRCDIR}" "${SRCDIR}" )
        ELSE()
            FILE(GLOB children RELATIVE "${SRCDIR}" "${SRCDIR}/*" )
            # Add subdirectories
            FOREACH(child ${children})
                FILE(GLOB_RECURSE SRCS "${SRCDIR}/${child}/*.cpp" "${SRCDIR}/${child}/*.cc" "${SRCDIR}/${child}/*.c" )
                LIST(LENGTH SRCS len)
                IF ( (IS_DIRECTORY ${SRCDIR}/${child}) AND (len GREATER 0) )
                    ADD_CPPCHECK_TEST_RECURSIVE( ${TESTNAME}-${child} "${SRCDIR}/${child}" )
                ENDIF()
            ENDFOREACH()
            # Add source files in current directory
            FILE(GLOB SRCS "${SRCDIR}/*.cpp" "${SRCDIR}/*.cc" "${SRCDIR}/*.c" )
            LIST(LENGTH SRCS len)
            IF ( len GREATER 0 )
                ADD_CPPCHECK_TEST( ${TESTNAME} "${SRCDIR}" ${SRCS} )
            ENDIF()
        ENDIF()
    ENDIF()
ENDFUNCTION()

# Add the test(s)
IF ( CPPCHECK )
    ADD_CPPCHECK_TEST_RECURSIVE( cppcheck "${CPPCHECK_SOURCE}" )
ENDIF()
