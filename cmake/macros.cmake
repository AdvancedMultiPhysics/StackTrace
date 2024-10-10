# Set compiler flags
MACRO( SET_COMPILER_FLAGS )
    IF ( CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU") )
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Woverloaded-virtual -Wsign-compare -Wformat-security -Wformat-overflow=2 -Wno-aggressive-loop-optimizations")
    ELSEIF ( MSVC OR MSVC_IDE OR MSVC60 OR MSVC70 OR MSVC71 OR MSVC80 OR CMAKE_COMPILER_2005 OR MSVC90 OR MSVC10 )
        IF ( NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
            MESSAGE( FATAL_ERROR "Using microsoft compilers on non-windows system?" )
        ENDIF()
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D _SCL_SECURE_NO_WARNINGS /D _CRT_SECURE_NO_WARNINGS /D _ITERATOR_DEBUG_LEVEL=0 /wd4267" )
    ELSEIF ( ${CMAKE_CXX_COMPILER_ID} MATCHES "Intel" ) 
        SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall" )
    ELSEIF ( ${CMAKE_CXX_COMPILER_ID} MATCHES "PGI" )
        SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Minform=inform -Mlist --display_error_number --diag_suppress 111,128,185")
    ELSEIF ( (${CMAKE_CXX_COMPILER_ID} MATCHES "CRAY") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Cray") )
        SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS}")
    ELSEIF ( (${CMAKE_CXX_COMPILER_ID} MATCHES "CLANG") OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang") )
        SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-missing-braces -Wmissing-field-initializers -ftemplate-depth=1024")
    ELSEIF ( ${CMAKE_CXX_COMPILER_ID} MATCHES "XL" )
        SET(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -Wall -ftemplate-depth=512")
    ELSE()
        MESSAGE( "CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
        MESSAGE( "CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
        MESSAGE(FATAL_ERROR "Unknown C/C++ compiler")
    ENDIF()
    MESSAGE( "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
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
    ELSE()
        SET( SYSTEM_LIBS "-ldl" )
        # Check for pthread
        CHECK_CXX_COMPILER_FLAG( "-pthread" pthread )
        CHECK_CXX_COMPILER_FLAG( "-lpthread" lpthread )
        IF ( pthread )
            SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread" )
        ENDIF()
        IF ( lpthread )
            LIST(APPEND SYSTEM_LIBS "-lpthread" )
        ENDIF()
        # Check for rdynamic
        # Needed for backtrace to print function names
        IF ( NOT USE_STATIC )
            CHECK_CXX_COMPILER_FLAG( "-rdynamic" rdynamic )
            IF ( rdynamic )
                LIST(APPEND SYSTEM_LIBS "$<$<LINK_LANGUAGE:CXX>:-rdynamic>" )
            ENDIF()
        ENDIF()
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
    TARGET_LINK_LIBRARIES( ${EXE} ${TIMER_LIB} ${MPI_CXX_LIBRARIES} ${COVERAGE_LIBS} ${SYSTEM_LIBS} )
    SET_TARGET_PROPERTIES( ${EXE} PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS} ${LDFLAGS} ${LDFLAGS_EXTRA}" )
    TARGET_COMPILE_DEFINITIONS(  ${EXE} PUBLIC ${COVERAGE_FLAGS} )
    INSTALL( TARGETS ${EXE} DESTINATION "${${PROJ}_INSTALL_DIR}/bin" )
ENDFUNCTION()
