# FindCppcheck
# ---------
#
# Find cppcheck
#
# Use this module by invoking find_package with the form:
#   find_package( Cppcheck )
#
# This module finds cppcheck and configures a test using the provided options
#
# This program recognizes the following options
#   CPPCHECK_SOURCE       - Source path to check
#   CPPCHECK_INCLUDE      - List of include folders (will overwrite defaults)
#   CPPCHECK_IGNORE       - List of files/folders to exclude
#   CPPCHECK_USE_JSON     - Use JSON file instead of search for files (recommended, disabled by default)
#   CPPCHECK_INCONCLUSIVE - Do we want to include the inclusive flag (disabled by default)
#   CPPCHECK_TIMEOUT      - Timeout for each cppcheck test (default is 5 minutes)
#   CPPCHECK_PROCS        - Number of processors to use (only used with CPPCHECK_USE_JSON)
#   CPPCHECK_OPTIONS      - List of cppcheck options (expert use: will overwrite default options)
#
# The following variables are set by find_package( Cppcheck )
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
    RETURN()
ENDIF()


# Set the source directory (if not set)
IF ( NOT DEFINED CPPCHECK_SOURCE )
    IF ( DEFINED ${PROJ}_SOURCE_DIR )
        SET( CPPCHECK_SOURCE "${${PROJ}_SOURCE_DIR}" )
    ELSE()
        SET( CPPCHECK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
    ENDIF()
ENDIF()


# Set the global ignore files/directories
SET( CPPCHECK_IGNORE_FILES )
SET( CPPCHECK_IGNORE_DIRECTORIES )
FOREACH ( tmp ${CPPCHECK_IGNORE} )
    IF ( EXISTS "${CPPCHECK_SOURCE}/${tmp}" )
        SET( tmp "${CPPCHECK_SOURCE}/${tmp}" )
    ENDIF()
    IF ( IS_DIRECTORY "${tmp}" )
        SET( CPPCHECK_IGNORE_DIRECTORIES ${CPPCHECK_IGNORE_DIRECTORIES} "${tmp}" )
    ELSE()
        SET( CPPCHECK_IGNORE_FILES ${CPPCHECK_IGNORE_FILES} "${tmp}" )
    ENDIF()
ENDFOREACH()


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
        SET( CPPCHECK_OPTIONS -q --error-exitcode=2 --enable=warning,performance,portability,information )
        IF ( CPPCHECK_INCONCLUSIVE )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --inconclusive )
        ENDIF()
        IF ( CPPCHECK_SUPPRESSION_FILE )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${CPPCHECK_SUPPRESSION_FILE}" )
        ELSEIF ( EXISTS "${CPPCHECK_SOURCE}/cppcheckSuppressionFile" )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${CPPCHECK_SOURCE}/cppcheckSuppressionFile" )
        ELSEIF ( EXISTS "${${PROJ}_SOURCE_DIR}/cppcheckSuppressionFile" )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${${PROJ}_SOURCE_DIR}/cppcheckSuppressionFile" )
        ELSEIF ( EXISTS "${PROJECT_SOURCE_DIR}/cppcheckSuppressionFile" )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${PROJECT_SOURCE_DIR}/cppcheckSuppressionFile" )
        ELSEIF ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cppcheckSuppressionFile" )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/cppcheckSuppressionFile" )
        ENDIF()
        IF ( CMAKE_C_STANDARD STREQUAL 99 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c99 )
        ELSEIF ( CMAKE_C_STANDARD STREQUAL 11 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c11 )
        ENDIF()
        IF ( CMAKE_CXX_STANDARD STREQUAL 98 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++03 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 11 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++11 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 14 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++14 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 17 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++17 )
        ELSEIF ( CMAKE_CXX_STANDARD STREQUAL 20 )
            SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} --std=c++20 )
        ENDIF()
        # Set definitions
        SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} -D__cppcheck__ )
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
    IF( NOT DEFINED CPPCHECK_INCLUDE AND NOT CPPCHECK_USE_JSON )
        SET( CPPCHECK_INCLUDE )
        IF ( PROCESSED )
            GET_PROPERTY( dirs DIRECTORY "${SRCDIR}" PROPERTY INCLUDE_DIRECTORIES )
        ELSE()
            GET_PROPERTY( dirs DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES )
        ENDIF()
        LIST( REMOVE_DUPLICATES dirs )
        SET( CPPCHECK_INCLUDE ${dirs} )
    ENDIF()
    FOREACH ( dir ${CPPCHECK_INCLUDE} )
        SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "-I${dir}" )
    ENDFOREACH()

    # Add the exclusions
    FOREACH ( tmp ${CPPCHECK_IGNORE_FILES} ${CPPCHECK_IGNORE_DIRECTORIES} )
        SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "-i${tmp}" )
    ENDFOREACH()

    # Set the timeout
    IF ( NOT DEFINED CPPCHECK_TIMEOUT )
        SET( CPPCHECK_TIMEOUT 300 )
    ENDIF()

    # Set the cppcheck build directory
    IF ( CPPCHECK_USE_JSON AND CMAKE_EXPORT_COMPILE_COMMANDS )
        STRING( REPLACE "cppcheck-" "" CPPCHECK_BUILD "${TESTNAME}" )
        SET( CPPCHECK_BUILD "${CMAKE_CURRENT_BINARY_DIR}/cppcheck-build/${CPPCHECK_BUILD}" )
    ELSE()
        SET( CPPCHECK_BUILD "${CMAKE_CURRENT_BINARY_DIR}/cppcheck-build" )
    ENDIF()
    FILE( MAKE_DIRECTORY ${CPPCHECK_BUILD} )
    SET( CPPCHECK_OPTIONS ${CPPCHECK_OPTIONS} "--cppcheck-build-dir=${CPPCHECK_BUILD}" )

    # Add the test
    ADD_TEST( ${TESTNAME} ${CPPCHECK} ${CPPCHECK_OPTIONS} ${ARGN} )
    SET_TESTS_PROPERTIES( ${TESTNAME} PROPERTIES PROCESSORS ${CPPCHECK_PROCS} TIMEOUT ${CPPCHECK_TIMEOUT} COST ${CPPCHECK_TIMEOUT} )
ENDFUNCTION()


# Add the cppcheck test splitting directories with too many files
FUNCTION( ADD_CPPCHECK_TEST_RECURSIVE TESTNAME SRCDIR )

    LIST( LENGTH SRCDIR src_len )
    IF ( src_len GREATER 1 )
        # Multiple src directories
        FOREACH ( src ${SRCDIR} )
            FILE( GLOB child RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${src}" )
            ADD_CPPCHECK_TEST_RECURSIVE( ${TESTNAME}-${child} "${src}" )
        ENDFOREACH()
    ELSE()
        # Get a list of all CMake subdirectories
        GET_PROPERTY( subdirs DIRECTORY "${SRCDIR}" PROPERTY SUBDIRECTORIES )
        # Get a list of all srcs
        FILE( GLOB_RECURSE SRCS "${SRCDIR}/*.cpp" "${SRCDIR}/*.cc" "${SRCDIR}/*.c" )
        # Remove excluded directories/files
        SET( EXCLUDED_FILES ${CPPCHECK_IGNORE_FILES} )
        FOREACH ( tmp ${CPPCHECK_IGNORE_DIRECTORIES} )
            FILE( GLOB_RECURSE tmp2 "${tmp}/*.cpp" "${tmp}/*.cc" "${tmp}/*.c" )
            SET( EXCLUDED_FILES ${CPPCHECK_IGNORE_FILES} ${tmp2} )
        ENDFOREACH()
        IF ( SRCS AND EXCLUDED_FILES )
            LIST( REMOVE_ITEM SRCS ${EXCLUDED_FILES} )
        ENDIF()
        # Check the number of srcs
        LIST( LENGTH SRCS len )
        IF ( len EQUAL 0 )
            # Nothing to process
        ELSEIF ( len LESS 20 )
            # Only a few files to process
            ADD_CPPCHECK_TEST( ${TESTNAME} "${SRCDIR}" "${SRCDIR}" )
        ELSEIF ( NOT subdirs )
            # No subdirectories
            ADD_CPPCHECK_TEST( ${TESTNAME} "${SRCDIR}" "${SRCDIR}" )
        ELSE()
            # Process each subdirectory
            FOREACH( dir ${subdirs} )
                STRING(REGEX REPLACE "${SRCDIR}." "${TESTNAME}-" TESTNAME2 ${dir} )
                ADD_CPPCHECK_TEST_RECURSIVE( ${TESTNAME2} "${dir}" )
            ENDFOREACH()
            # Find any files that are not part of the included subdirectories
            FOREACH( dir ${subdirs} )
                LIST( FILTER SRCS EXCLUDE REGEX "${dir}" )
            ENDFOREACH()
            LIST(LENGTH SRCS len)
            # Set the maximum number of files in a call
            IF ( NOT CPPCHECK_MAX_FILES )
                SET( CPPCHECK_MAX_FILES 50 )
            ENDIF()
            # Break the files into chunks and call cppcheck
            IF ( NOT SRCS )
                # No files
            ELSEIF ( len LESS ${CPPCHECK_MAX_FILES} )
                ADD_CPPCHECK_TEST( ${TESTNAME} "${SRCDIR}" ${SRCS} )
            ELSE()
                SET( INDEX 1 )
                WHILE ( SRCS )
                    SET( SRCS2 )
                    SET( COUNT 1 )
                    WHILE( SRCS AND ( COUNT LESS ${CPPCHECK_MAX_FILES} ) )
                        LIST(GET SRCS 0 ITEM )
                        SET( SRCS2 ${SRCS2} ${ITEM} )
                        LIST(REMOVE_AT SRCS 0 )
                        MATH( EXPR COUNT "${COUNT}+1" )
                    ENDWHILE()
                    ADD_CPPCHECK_TEST( ${TESTNAME}-${INDEX} "${SRCDIR}" ${SRCS2} )
                    MATH( EXPR INDEX "${INDEX}+1" )
                ENDWHILE()
            ENDIF()
        ENDIF()
    ENDIF()
ENDFUNCTION()


# Add the test(s)
IF ( CPPCHECK_USE_JSON AND CMAKE_EXPORT_COMPILE_COMMANDS )
    IF ( NOT CPPCHECK_PROCS )
        SET( CPPCHECK_PROCS 8 )
    ENDIF()
    IF ( "${TEST_MAX_PROCS}" LESS "${CPPCHECK_PROCS}" )
        SET( CPPCHECK_PROCS ${TEST_MAX_PROCS} )
    ENDIF()
    IF ( "${CPPCHECK_PROCS}" GREATER "1" )
        ADD_CPPCHECK_TEST( cppcheck "${SRCDIR}" "--project=${CMAKE_BINARY_DIR}/compile_commands.json" -j ${CPPCHECK_PROCS} )
    ELSE()
        SET( CPPCHECK_PROCS 1 )
        ADD_CPPCHECK_TEST( cppcheck "${SRCDIR}" "--project=${CMAKE_BINARY_DIR}/compile_commands.json" )
    ENDIF()
ELSE()
    SET( CPPCHECK_PROCS 1 )
    ADD_CPPCHECK_TEST_RECURSIVE( cppcheck "${CPPCHECK_SOURCE}" )
ENDIF()


