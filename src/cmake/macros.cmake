# Macro to set the proper warnings
MACRO( SET_WARNINGS )
  IF ( USING_GCC )
    # Add gcc specific compiler options
    # Note: adding -Wlogical-op causes a wierd linking error on Titan using the nvcc wrapper:
    #    /usr/bin/ld: cannot find gical-op: No such file or directory
    SET(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wall -Wextra") 
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Woverloaded-virtual -Wsign-compare -Wformat-security")
  ELSEIF ( USING_MSVC )
    # Add Microsoft specifc compiler options
    SET(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267" )
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267" )
  ELSEIF ( USING_ICC )
    # Add Intel specifc compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -Wall" )
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -wd1011" )
  ELSEIF ( USING_CRAY )
    # Add default compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS}")
  ELSEIF ( USING_PGCC )
    # Add default compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -lpthread")
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -lpthread -Minform=inform -Mlist --display_error_number")
    # Suppress unreachable code warning, it causes non-useful warnings with some tests/templates
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} --diag_suppress 111,128,185")
  ELSEIF ( USING_CLANG )
    # Add default compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -Wall -Wextra")
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-missing-braces -Wmissing-field-initializers -ftemplate-depth=1024")
  ELSEIF ( USING_XL )
    # Add default compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -Wall")
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -ftemplate-depth=512")
  ELSE ( )
    MESSAGE("Compiler specific features are not set for this compiler")
  ENDIF()
ENDMACRO()


# Macro to add user compile flags
MACRO( ADD_USER_FLAGS )
    STRING( STRIP "${CMAKE_C_FLAGS} ${CFLAGS} ${CFLAGS_EXTRA}" CMAKE_C_FLAGS )
    STRING( STRIP "${CMAKE_CXX_FLAGS} ${CXXFLAGS} ${CXXFLAGS_EXTRA}" CMAKE_CXX_FLAGS )
    STRING( STRIP "${CMAKE_Fortran_FLAGS} ${FFLAGS} ${FFLAGS_EXTRA}" CMAKE_Fortran_FLAGS )
    STRING( STRIP "${LDFLAGS} ${LDFLAGS_EXTRA}" LDFLAGS )
    STRING( STRIP "${LDLIBS} ${LDLIBS_EXTRA}" LDLIBS )
ENDMACRO()


# Macro to check if a flag is enabled
MACRO( CHECK_ENABLE_FLAG FLAG DEFAULT )
    IF( NOT DEFINED ${FLAG} )
        SET( ${FLAG} ${DEFAULT} )
    ELSEIF( ${FLAG}  STREQUAL "" )
        SET( ${FLAG} ${DEFAULT} )
    ELSEIF( ( ${${FLAG}} STREQUAL "FALSE" ) OR ( ${${FLAG}} STREQUAL "false" ) OR ( ${${FLAG}} STREQUAL "0" ) OR ( ${${FLAG}} STREQUAL "OFF" ) )
        SET( ${FLAG} 0 )
    ELSEIF( ( ${${FLAG}} STREQUAL "TRUE" ) OR ( ${${FLAG}} STREQUAL "true" ) OR ( ${${FLAG}} STREQUAL "1" ) OR ( ${${FLAG}} STREQUAL "ON" ) )
        SET( ${FLAG} 1 )
    ELSE()
        MESSAGE( "Bad value for ${FLAG} (${${FLAG}}); use true or false" )
    ENDIF()
ENDMACRO()


# Get system libraries
MACRO( GET_SYSTEM_LIBS )
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        # Windows specific system libraries
        SET( SYSTEM_PATHS "C:/Program Files (x86)/Microsoft SDKs/Windows/v7.0A/Lib/x64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 8/VC/PlatformSDK/Lib/AMD64" 
                          "C:/Program Files (x86)/Microsoft Visual Studio 12.0/Common7/Packages/Debugger/X64" )
        FIND_LIBRARY( PSAPI_LIB    NAMES Psapi    PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp  PATHS ${SYSTEM_PATHS}  NO_DEFAULT_PATH )
        FIND_LIBRARY( DBGHELP_LIB  NAMES DbgHelp )
        IF ( PSAPI_LIB ) 
            ADD_DEFINITIONS( -DPSAPI )
            SET( SYSTEM_LIBS ${PSAPI_LIB} )
        ENDIF()
        IF ( DBGHELP_LIB ) 
            ADD_DEFINITIONS( -DDBGHELP )
            SET( SYSTEM_LIBS ${DBGHELP_LIB} )
        ELSE()
            MESSAGE( WARNING "Did not find DbgHelp, stack trace will not be availible" )
        ENDIF()
        MESSAGE("System libs: ${SYSTEM_LIBS}")
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
        # Linux specific system libraries
        SET( SYSTEM_LIBS "-ldl -lpthread" )
        IF ( NOT USE_STATIC )
            SET( SYSTEM_LIBS "${SYSTEM_LIBS} -rdynamic" )   # Needed for backtrace to print function names
        ENDIF()
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin" )
        # Max specific system libraries
        SET( SYSTEM_LIBS "-ldl -lpthread" )
    ELSEIF( ${CMAKE_SYSTEM_NAME} STREQUAL "Generic" )
        # Generic system libraries
    ELSE()
        MESSAGE( FATAL_ERROR "OS not detected" )
    ENDIF()
ENDMACRO()


# add custom target distclean
# cleans and removes cmake generated files etc.
MACRO( ADD_DISTCLEAN ${ARGN} )
    SET(DISTCLEANED
        assembly
        cmake.depends
        cmake.check_depends
        CMakeCache.txt
        CMakeFiles
        CMakeTmp
        CMakeDoxy*
        cmake.check_cache
        *.cmake
        compile.log
        Doxyfile
        Makefile
        core core.*
        DartConfiguration.tcl
        install_manifest.txt
        Testing
        include
        doc
        docs
        examples
        latex_docs
        lib
        Makefile.config
        install_manifest.txt
        test
        matlab
        Matlab
        mex
        tmp
        #tmp#
        bin
        cmake
        cppclean
        compile_commands.json
        ${ARGN}
    )
    ADD_CUSTOM_TARGET(distclean @echo cleaning for source distribution)
    IF (UNIX)
        ADD_CUSTOM_COMMAND(
            DEPENDS clean
            COMMENT "distribution clean"
            COMMAND rm
            ARGS    -Rf ${DISTCLEANED}
            TARGET  distclean
        )
    ELSE()
        SET( DISTCLEANED
            ${DISTCLEANED}
            *.vcxproj*
            ipch
            x64
            Debug
        )
        SET( DISTCLEAN_FILE "${CMAKE_CURRENT_BINARY_DIR}/distclean.bat" )
        FILE( WRITE  "${DISTCLEAN_FILE}" "del /s /q /f " )
        APPEND_LIST( "${DISTCLEAN_FILE}" "${DISTCLEANED}" " " " " )
        FILE( APPEND "${DISTCLEAN_FILE}" "\n" )
        APPEND_LIST( "${DISTCLEAN_FILE}" "${DISTCLEANED}" "for /d %%x in ("   ") do rd /s /q \"%%x\"\n" )
        ADD_CUSTOM_COMMAND(
            DEPENDS clean
            COMMENT "distribution clean"
            COMMAND distclean.bat & del /s/q/f distclean.bat
            TARGET  distclean
        )
    ENDIF()
ENDMACRO()


# Add a test
FUNCTION( ADD_EXE EXE SRC )
    ADD_EXECUTABLE( ${EXE} ${SRC} )
    TARGET_LINK_LIBRARIES( ${EXE} stacktrace )
    TARGET_LINK_LIBRARIES( ${EXE} ${TIMER_LIB} ${MPI_CXX_LIBRARIES} ${COVERAGE_LIBS} "${SYSTEM_LIBS}" )
    SET_TARGET_PROPERTIES( ${EXE} PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS} ${LDFLAGS} ${LDFLAGS_EXTRA}" )
    TARGET_COMPILE_DEFINITIONS(  ${EXE} PUBLIC ${COVERAGE_FLAGS} )
    INSTALL( TARGETS ${EXE} DESTINATION "${STACKTRACE_INSTALL_DIR}/bin" )
ENDFUNCTION()
