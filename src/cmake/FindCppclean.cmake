# FindCppclean
# ---------
#
# Find cppclean
#
# Use this module by invoking find_package with the form:
#
#   find_package( Cppclean
#     [REQUIRED]                # Fail with error if the cppclean is not found
#   )
#
# This module finds cppclean and configures a test using the provided options
#
# This program reconizes the following options
#   CPPCLEAN_INCLUDE            - List of include folders
#   CPPCLEAN_OPTIONS            - List of cppclean options
#   CPPCLEAN_EXCLUDE            - List of files to exclude
#   CPPCLEAN_SUPPRESSIONS       - List of messages to suppress (based on partial string matching)
#   CPPCLEAN_UNNECESSARY_INCLUDE- Enable warnings about unnecessary includes
#   CPPCLEAN_EXTRA_INCLUDE      - Enable warnings about extra includes
#   CPPCLEAN_SHOULD_INCLUDE     - Enable warnings about should includes
#   CPPCLEAN_INCLUDE_NOT_FOUND  - Enable warnings about missing includes
#   CPPCLEAN_FUN_NOT_FOUND      - Enable warnings about functions not found
#   CPPCLEAN_DECLARED           - Enable warnings about declared but not defined
#   CPPCLEAN_STATIC             - Enable warnings about static data
#   CPPCLEAN_UNUSED_VARIABLE    - Enable warnings about unused variables
#   CPPCLEAN_FORWARD_DECLARE    - Enable messages for forward decelerations
#   CPPCLEAN_UNKNOWN            - Enable unknown messages
#   CPPCLEAN_SOURCE             - Source path to check
#   CPPCLEAN_FAIL_ON_WARNINGS   - Cause the test to fail if warnings are detected (default is false)
#
# The following variables are set by find_package( Cppclean )
#
#   CPPCLEAN_FOUND              - True if cppclean was found


# Find cppclean if availible
FIND_PROGRAM( CPPCLEAN
    NAMES cppclean cppclean.exe 
    PATHS "${CPPCLEAN_DIRECTORY}" "C:/Program Files/Cppclean" "C:/Program Files (x86)/Cppclean" 
)
IF ( CPPCLEAN )
    SET( CPPCLEAN_FOUND TRUE )
ELSE()
    SET( CPPCLEAN_FOUND FALSE )
ENDIF()
IF ( CPPCLEAN_FOUND )
    MESSAGE(STATUS "Using cppclean")
ELSEIF ( CPPCLEAN_FIND_REQUIRED )
    MESSAGE( FATAL_ERROR "cppclean not found")
ELSE()
    MESSAGE( STATUS "cppclean not found")
ENDIF()
IF ( NOT DEFINED CPPCLEAN_FAIL_ON_WARNINGS )
    SET( CPPCLEAN_FAIL_ON_WARNINGS 0 )
ENDIF()


# Set the options for cppclean
IF ( NOT DEFINED CPPCLEAN_INCLUDE )
    SET( CPPCLEAN_INCLUDE )
    GET_PROPERTY( dirs DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY INCLUDE_DIRECTORIES )
    FOREACH(dir ${dirs})
        SET( CPPCLEAN_INCLUDE ${CPPCLEAN_INCLUDE} "${dir}" )
    ENDFOREACH()
ENDIF()
IF ( NOT DEFINED CPPCLEAN_TIMEOUT )
    SET( CPPCLEAN_TIMEOUT 250 )
ENDIF()


# Helper function
FUNCTION( CPPCLEAN_PRINT_VAR FILENAME VAR )
    IF ( CPPCLEAN_${VAR} )
        FILE(APPEND "${FILENAME}" "MESSAGE(STATUS \"${VAR}:\")\n" )
        FILE(APPEND "${FILENAME}" "FOREACH( line \${${VAR}} )\n" )
        FILE(APPEND "${FILENAME}" "   MESSAGE(\${line})\n" )
        FILE(APPEND "${FILENAME}" "ENDFOREACH()\n" )
        FILE(APPEND "${FILENAME}" "MESSAGE(\"\")\n" )
        FILE(APPEND "${FILENAME}" "LIST(LENGTH ${VAR} len)\nMATH(EXPR ERR \"\${ERR}+\${len}\")\n" )
    ENDIF()
ENDFUNCTION()


# Add the test
IF ( CPPCLEAN )
    CHECK_ENABLE_FLAG( CPPCLEAN_UNNECESSARY_INCLUDE 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_EXTRA_INCLUDE 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_SHOULD_INCLUDE 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_INCLUDE_NOT_FOUND 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_FUN_NOT_FOUND 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_DECLARED 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_STATIC 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_FORWARD_DECLARE 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_UNUSED_VARIABLE 1 )
    CHECK_ENABLE_FLAG( CPPCLEAN_UNKNOWN 1 )
    # Create a script to run cppclean and check the results
    SET( CPPCLEAN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}" )
    SET( CPPCLEAN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/cppclean/output.txt" )
    SET( CPPCLEAN_ERROR  "${CMAKE_CURRENT_BINARY_DIR}/cppclean/error.txt" )
    SET( FILENAME "${CMAKE_CURRENT_BINARY_DIR}/cppclean/script.cmake" )
    CONFIGURE_FILE( "${CMAKE_CURRENT_LIST_DIR}/run.cppclean.template.cmake" "${FILENAME}" @ONLY )
    # Create the test
    ADD_TEST( cppclean ${CMAKE_COMMAND} -P "${FILENAME}" )
    IF ( ${CPPCLEAN_FAIL_ON_WARNINGS} )
        SET_TESTS_PROPERTIES( cppclean PROPERTIES PROCESSORS 1 PASS_REGULAR_EXPRESSION "All tests passed" )
    ELSE()
        SET_TESTS_PROPERTIES( cppclean PROPERTIES PROCESSORS 1 )
    ENDIF()
ENDIF()

