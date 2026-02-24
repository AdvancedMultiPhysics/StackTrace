# - Run Doxygen
#
# Adds a doxygen target that runs doxygen to generate the html
# and optionally the LaTeX API documentation.
# The doxygen target is added to the doc target as a dependency.
# i.e.: the API documentation is built with:
#  make doc
#
# USAGE: GLOBAL INSTALL
#
# Install it with:
#  cmake ./ && sudo make install
# Add the following to the CMakeLists.txt of your project:
#  include(UseDoxygen OPTIONAL)
# Optionally copy Doxyfile.in in the directory of CMakeLists.txt and edit it.
#
# USAGE: INCLUDE IN PROJECT
#
#  set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#  include(UseDoxygen)
# Add the Doxyfile.in and UseDoxygen.cmake files to the projects source directory.
#
#
# Variables you may define are:
#  DOXYFILE_SOURCE_DIR - Path where the Doxygen input files are.
#  	Defaults to the current source and binary directory.
#  DOXYFILE_OUTPUT_DIR - Path where the Doxygen output is stored. Defaults to "doc".
#
#  DOXYFILE_LATEX - Set to "NO" if you do not want the LaTeX documentation
#  	to be built.
#  DOXYFILE_LATEX_DIR - Directory relative to DOXYFILE_OUTPUT_DIR where
#  	the Doxygen LaTeX output is stored. Defaults to "latex".
#
#  DOXYFILE_HTML_DIR - Directory relative to DOXYFILE_OUTPUT_DIR where
#  	the Doxygen html output is stored. Defaults to "html".
#

#
#  Copyright (c) 2009, 2010 Tobias Rautenkranz <tobias@rautenkranz.ch>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

MACRO( usedoxygen_set_default name value )
    IF ( NOT DEFINED "${name}" )
        SET( "${name}" "${value}" )
    ENDIF()
ENDMACRO()

FIND_PACKAGE( Doxygen )

IF ( DOXYGEN_FOUND )
    FIND_FILE( DOXYFILE_IN "Doxyfile.in" PATHS "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_ROOT}/Modules/" NO_DEFAULT_PATH )
    SET( DOXYFILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" )
    INCLUDE( FindPackageHandleStandardArgs )
    FIND_PACKAGE_HANDLE_STANDARD_ARGS( DOXYFILE_IN DEFAULT_MSG "DOXYFILE_IN" )
ENDIF()

IF ( DOXYGEN_FOUND AND DOXYFILE_IN_FOUND )
    USEDOXYGEN_SET_DEFAULT( DOXYFILE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc" )
    USEDOXYGEN_SET_DEFAULT( DOXYFILE_HTML_DIR "html" )
    USEDOXYGEN_SET_DEFAULT( DOXYFILE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}\"
		\"${CMAKE_CURRENT_BINARY_DIR}" )

    SET_PROPERTY( DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_HTML_DIR}" )

    ADD_CUSTOM_TARGET( doxygen COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE} COMMENT "Writing documentation to ${DOXYFILE_OUTPUT_DIR}..." WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    ## LaTeX
    SET( DOXYFILE_PDFLATEX "NO" )
    SET( DOXYFILE_DOT "NO" )

    FIND_PACKAGE( LATEX )
    FIND_PROGRAM( MAKE_PROGRAM make )
    IF ( LATEX_COMPILER AND MAKEINDEX_COMPILER AND MAKE_PROGRAM AND ( NOT DEFINED DOXYFILE_LATEX OR DOXYFILE_LATEX STREQUAL "YES" ) )
        SET( DOXYFILE_LATEX "YES" )
        USEDOXYGEN_SET_DEFAULT( DOXYFILE_LATEX_DIR "latex" )

        SET_PROPERTY( DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}" )

        IF ( PDFLATEX_COMPILER )
            SET( DOXYFILE_PDFLATEX "YES" )
        ENDIF()
        IF ( DOXYGEN_DOT_EXECUTABLE )
            SET( DOXYFILE_DOT "YES" )
        ENDIF()

        ADD_CUSTOM_COMMAND( TARGET doxygen POST_BUILD COMMAND ${MAKE_PROGRAM} COMMENT "Running LaTeX for Doxygen documentation in ${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}..."
                           WORKING_DIRECTORY "${DOXYFILE_OUTPUT_DIR}/${DOXYFILE_LATEX_DIR}" )
    ELSE()
        SET( DOXYGEN_LATEX "NO" )
    ENDIF()

    CONFIGURE_FILE( ${DOXYFILE_IN} Doxyfile IMMEDIATE @ONLY )

    GET_TARGET_PROPERTY( DOC_TARGET doc TYPE )
    IF ( NOT DOC_TARGET )
        ADD_CUSTOM_TARGET( doc )
    ENDIF()

    ADD_DEPENDENCIES( doc doxygen )
ENDIF()
