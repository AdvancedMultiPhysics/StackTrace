# FindCoverage
# ---------
#
# Find Coverage flags and libraries
#
# Use this module by invoking find_package with the form:
#
#   find_package( FindCoverage
#     [REQUIRED]             # Fail with error if the cppcheck is not found
#   )
#
# This module finds headers and requested component libraries for the TPLs that
# were installed by the builder.  
#
#   COVERAGE_FOUND            - True if coverage was found
#   COVERAGE_FLAGS             - Compiler flags for coverage
#   COVERAGE_LIBS              - Coverage libraries


# Some preliminary stuff
GET_PROPERTY(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
INCLUDE(CheckCCompilerFlag)
INCLUDE(CheckCXXCompilerFlag)
IF ( ";${languages};" MATCHES ";Fortran;" )
    INCLUDE(CheckFortranCompilerFlag)
ENDIF()


# Test for gcov
SET( CMAKE_REQUIRED_FLAGS ${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage )
CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" profile-arcs )
IF ( profile-arcs )
    SET( COVERAGE_FOUND true )
    SET( COVERAGE_FLAGS "-DUSE_GCOV -fprofile-arcs -ftest-coverage" )
    SET( COVERAGE_LIBS -fprofile-arcs )
ENDIF()
FIND_LIBRARY( GCOV_LIB  NAMES gcov  )
IF ( NOT GCOV_LIB )
    SET( GCOV_LIB )
    SET( CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS} ${LDFLAGS} ${LDFLAGS_EXTRA} -lgcov" )
    CHECK_CXX_SOURCE_COMPILES( "int main() { return 0;}" lgcov )
    IF ( lgcov )
        SET( GCOV_LIB -lgcov )
    ENDIF()
ENDIF()
#IF ( NOT GCOV_LIB AND ";${languages};" MATCHES ";Fortran;" )
#    SET( GCOV_LIB )
#    SET( CMAKE_REQUIRED_FLAGS "${CMAKE_Fortran_FLAGS} ${COVERAGE_FLAGS} -lgcov" )
#    CHECK_Fortran_SOURCE_COMPILES( "       program hello\n       end program hello\n" lgcov-fortran )
#    IF ( lgcov-fortran )
#        SET( GCOV_LIB -lgcov )
#    ENDIF()
#ENDIF()
IF ( GCOV_LIB )
    SET( COVERAGE_LIBS ${GCOV_LIB} ${COVERAGE_LIBS} )
ENDIF()


# Check if coverage was found
IF ( COVERAGE_FIND_REQUIRED AND NOT COVERAGE_FOUND )
    MESSAGE( FATAL_ERROR "Coverage was not found" )
ENDIF()


# Print the coverage flags
IF ( NOT TPLs_FIND_QUIETLY )
    IF ( COVERAGE_FOUND )
        MESSAGE("Coverage found:")
        MESSAGE("   COVERAGE_FLAGS = ${COVERAGE_FLAGS}")
        MESSAGE("   COVERAGE_LIBS = ${COVERAGE_LIBS}")
    ELSE()
        MESSAGE("Coverage not found:")
    ENDIF()
ENDIF()

