# Search for valgrind

# Get VALGRIND_ROOT (if set)
IF ( NOT VALGRIND_ROOT AND DEFINED ENV{VALGRIND_ROOT} )
	SET( VALGRIND_ROOT "$ENV{VALGRIND_ROOT}" )
endif()

# Find the executable
FIND_PROGRAM( Valgrind_EXECUTABLE NAMES valgrind PATHS "${VALGRIND_ROOT}" "${VALGRIND_ROOT}/bin" ) 
GET_FILENAME_COMPONENT( Valgrind_BINARY_PATH "${Valgrind_EXECUTABLE}" DIRECTORY )

# Find the include path
FIND_PATH( Valgrind_INCLUDE_DIR "valgrind.h" HINTS "${Valgrind_BINARY_PATH}"
                                                   "${Valgrind_BINARY_PATH}/valgrind"
                                                   "${Valgrind_BINARY_PATH}/include"
                                                   "${Valgrind_BINARY_PATH}/.."
                                                   "${Valgrind_BINARY_PATH}/../valgrind"
                                                   "${Valgrind_BINARY_PATH}/../include"
                                                   "${Valgrind_BINARY_PATH}/../include/valgrind"
)

# Find valgrind package
INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( Valgrind DEFAULT_MSG Valgrind_INCLUDE_DIR Valgrind_EXECUTABLE )

# Print the results (if found)
IF ( Valgrind_FOUND )
    MESSAGE( "   Valgrind_EXECUTABLE = ${Valgrind_EXECUTABLE}" )
    MESSAGE( "   Valgrind_INCLUDE_DIR = ${Valgrind_INCLUDE_DIR}" )
ENDIF()

