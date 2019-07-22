# Macro to identify the compiler
MACRO( IDENTIFY_COMPILER )
    # SET the C/C++ compiler
    IF ( CMAKE_C_COMPILER_WORKS OR CMAKE_CXX_COMPILER_WORKS )
        IF( USING_GCC OR CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR
            (${CMAKE_C_COMPILER_ID} MATCHES "GNU") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU") )
            SET( USING_GCC TRUE )
            ADD_DEFINITIONS( -DUSING_GCC )
            MESSAGE("Using gcc")
        ELSEIF( USING_MSVC OR MSVC OR MSVC_IDE OR MSVC60 OR MSVC70 OR MSVC71 OR MSVC80 OR CMAKE_COMPILER_2005 OR MSVC90 OR MSVC10 )
            IF( NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
                MESSAGE( FATAL_ERROR "Using microsoft compilers on non-windows system?" )
            ENDIF()
            SET( USING_MSVC TRUE )
            ADD_DEFINITIONS( -DUSING_MSVC )
            MESSAGE("Using Microsoft")
        ELSEIF( USING_ICC OR (${CMAKE_C_COMPILER_ID} MATCHES "Intel") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Intel") ) 
            SET(USING_ICC TRUE)
            ADD_DEFINITIONS( -DUSING_ICC )
            MESSAGE("Using icc")
        ELSEIF( USING_PGCC OR (${CMAKE_C_COMPILER_ID} MATCHES "PGI") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "PGI") )
            SET(USING_PGCC TRUE)
            ADD_DEFINITIONS( -DUSING_PGCC )
            MESSAGE("Using pgCC")
        ELSEIF( USING_CRAY OR (${CMAKE_C_COMPILER_ID} MATCHES "CRAY") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "CRAY") OR
                              (${CMAKE_C_COMPILER_ID} MATCHES "Cray") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Cray") )
            SET(USING_CRAY TRUE)
            ADD_DEFINITIONS( -DUSING_CRAY )
            MESSAGE("Using Cray")
        ELSEIF( USING_CLANG OR (${CMAKE_C_COMPILER_ID} MATCHES "CLANG") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "CLANG") OR
                               (${CMAKE_C_COMPILER_ID} MATCHES "Clang") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") )
            SET(USING_CLANG TRUE)
            ADD_DEFINITIONS( -DUSING_CLANG )
            MESSAGE("Using Clang")
        ELSEIF( (${CMAKE_C_COMPILER_ID} MATCHES "XL") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "XL") )
            SET(USING_XL TRUE)
            ADD_DEFINITIONS( -DUSING_XL )
            MESSAGE("Using XL")
        ELSE()
            MESSAGE( "CMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
            MESSAGE( "CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
            MESSAGE( "CMAKE_C_COMPILER_ID=${CMAKE_C_COMPILER_ID}")
            MESSAGE( "CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
            MESSAGE(FATAL_ERROR "Unknown C/C++ compiler")
        ENDIF()
    ENDIF()
    # SET the Fortran++ compiler
    IF ( CMAKE_Fortran_COMPILER_WORKS )
        IF( CMAKE_COMPILER_IS_GNUG77 OR (${CMAKE_Fortran_COMPILER_ID} MATCHES "GNU") )
            SET( USING_GFORTRAN TRUE )
            MESSAGE("Using gfortran")
        ELSEIF ( (${CMAKE_Fortran_COMPILER_ID} MATCHES "Intel") ) 
            SET(USING_IFORT TRUE)
            MESSAGE("Using ifort")
        ELSEIF ( ${CMAKE_Fortran_COMPILER_ID} MATCHES "PGI")
            SET(USING_PGF90 TRUE)
            MESSAGE("Using pgf90")
        ELSEIF ( (${CMAKE_Fortran_COMPILER_ID} MATCHES "CLANG") OR (${CMAKE_Fortran_COMPILER_ID} MATCHES "Clang") OR
                 (${CMAKE_Fortran_COMPILER_ID} MATCHES "FLANG") OR (${CMAKE_Fortran_COMPILER_ID} MATCHES "Flang") )
            SET(USING_FLANG TRUE)
            MESSAGE("Using flang")
        ELSE()
            MESSAGE( "CMAKE_Fortran_COMPILER=${CMAKE_Fortran_COMPILER}")
            MESSAGE( "CMAKE_Fortran_COMPILER_ID=${CMAKE_Fortran_COMPILER_ID}")
            MESSAGE(FATAL_ERROR "Unknown Fortran compiler (${CMAKE_Fortran_COMPILER_ID})")
        ENDIF()
    ENDIF()
ENDMACRO()


# Macro to set the proper warnings
MACRO( SET_WARNINGS )
  IF ( USING_GCC )
    # Add gcc specific compiler options
    # Note: adding -Wlogical-op causes a wierd linking error on Titan using the nvcc wrapper:
    #    /usr/bin/ld: cannot find gical-op: No such file or directory
    SET(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wall -Wextra") 
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Woverloaded-virtual -Wsign-compare -pedantic")
  ELSEIF ( USING_MSVC )
    # Add Microsoft specifc compiler options
    SET(CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267" )
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267" )
  ELSEIF ( USING_ICC )
    # Add Intel specifc compiler options
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -Wall" )
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall" )
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
    SET(CMAKE_C_FLAGS     " ${CMAKE_C_FLAGS} -Wall")
    SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -Wno-missing-braces -Wmissing-field-initializers -ftemplate-depth=1024")
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


# Macro to set the compile/link flags
MACRO( SET_COMPILER_FLAGS )
    # Initilaize the compiler
    IDENTIFY_COMPILER()
    # Set the default flags for each build type
    IF ( USING_MSVC )
        SET(CMAKE_C_FLAGS_DEBUG   "-D_DEBUG /DEBUG /Od /EHsc /MDd /Z7" )
        SET(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG /DEBUG /Od /EHsc /MDd /Z7" )
        SET(CMAKE_C_FLAGS_RELEASE   "/O2 /EHsc /MD" )
        SET(CMAKE_CXX_FLAGS_RELEASE "/O2 /EHsc /MD" )
    ELSE()
    ENDIF()
    # Set the behavior of GLIBCXX flags
    CHECK_ENABLE_FLAG( ENABLE_GXX_DEBUG 0 )
    IF ( ENABLE_GXX_DEBUG ) 
        # Enable GLIBCXX_DEBUG flags
        SET( CMAKE_C_FLAGS_DEBUG   " ${CMAKE_C_FLAGS_DEBUG}   -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC" )
        SET( CMAKE_CXX_FLAGS_DEBUG " ${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC" )
        SET( DISABLE_GXX_DEBUG OFF )
    ELSEIF ( DISABLE_GXX_DEBUG ) 
        # Disable GLIBCXX_DEBUG flags
        SET( DISABLE_GXX_DEBUG OFF )
    ELSE()
        # Default
        SET( DISABLE_GXX_DEBUG ON )
    ENDIF()
    # Set debug definitions
    IF ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND NOT ("${CMAKE_CXX_FLAGS_DEBUG}" MATCHES "-D_DEBUG") )
        SET( CMAKE_C_FLAGS_DEBUG   " ${CMAKE_C_FLAGS_DEBUG}   -DDEBUG -D_DEBUG" )
        SET( CMAKE_CXX_FLAGS_DEBUG " ${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG" )        
    ENDIF()
    # Save the debug/release specific flags to the cache
    SET( CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG}"     CACHE STRING "Debug flags"   FORCE)
    SET( CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}"   CACHE STRING "Release flags" FORCE)
    SET( CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}"   CACHE STRING "Debug flags"   FORCE)
    SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Release flags" FORCE)
    # Add the user flags
    ADD_USER_FLAGS()
    # Set the warnings to use
    SET_WARNINGS()
    # Test the compile flags
    IF ( CMAKE_C_COMPILER_WORKS )
        CHECK_C_COMPILER_FLAG( "${CMAKE_C_FLAGS}" CHECK_C_FLAGS )
    ENDIF()
    IF ( CMAKE_CXX_COMPILER_WORKS )
        CHECK_CXX_COMPILER_FLAG( "${CMAKE_CXX_FLAGS}" CHECK_CXX_FLAGS )
    ENDIF()
    IF ( ( NOT CHECK_C_FLAGS ) OR ( NOT CHECK_CXX_FLAGS ) )
        IF ( USING_CRAY )
            MESSAGE(WARNING "Invalid C/CXX flags detected:\n"
                "C flags: ${CMAKE_C_FLAGS}\n" "CXX flags: ${CMAKE_CXX_FLAGS}\n" )
        ENDIF()
    ENDIF()
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
        cmake.depends
        cmake.check_depends
        CMakeCache.txt
        CMakeFiles
        CMakeTmp
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
        latex_docs
        lib
        Makefile.config
        install_manifest.txt
        test
        matlab
        mex
        tmp
        #tmp#
        bin
        cmake
        cppclean
        compile_commands.json
        ${ARGN}
    )
    ADD_CUSTOM_TARGET (distclean @echo cleaning for source distribution)
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
    TARGET_COMPILE_DEFINITIONS(  ${EXE} PUBLIC ${COVERAGE_FLAGS} )
    INSTALL( TARGETS ${EXE} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" )
ENDFUNCTION()
