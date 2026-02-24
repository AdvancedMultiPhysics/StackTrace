FUNCTION( WRITE_REPO_VERSION )

    # Check the inputs
    IF ( NOT PROJ )
        MESSAGE( FATAL_ERROR "PROJ is not set" )
    ENDIF()
    IF ( NOT ${PROJ}_NAMESPACE )
        SET( ${PROJ}_NAMESPACE ${PROJ} )
    ENDIF()
    IF ( NOT ${PROJ}_SOURCE_DIR )
        MESSAGE( FATAL_ERROR "${PROJ}_SOURCE_DIR is not set" )
    ENDIF()
    IF ( NOT IS_DIRECTORY "${${PROJ}_SOURCE_DIR}" )
        MESSAGE( FATAL_ERROR "Source path \"${${PROJ}_SOURCE_DIR}\" does not exist" )
    ENDIF()
    SET( src_dir "${${PROJ}_SOURCE_DIR}" )

    # Set the output filename
    SET( filename "${${PROJ}_INSTALL_DIR}/include/${${PROJ}_INC}/${PROJ}_Version.h" )
    SET( extra_args ${ARGN} )
    LIST( LENGTH extra_args extra_count )
    IF ( ${extra_count} GREATER 0 )
        LIST( GET extra_args 0 optional_arg )
        SET( filename "${optional_arg}" )
    ENDIF()

    # Get version info
    GET_VERSION_INFO( ${PROJ} ${src_dir} )
    SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_BUILD_VERSION ${${PROJ}_BUILD_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_SHORT_HASH ${${PROJ}_SHORT_HASH} PARENT_SCOPE )
    SET( ${PROJ}_LONG_HASH ${${PROJ}_LONG_HASH} PARENT_SCOPE )
    SET( ${PROJ}_BRANCH ${${PROJ}_BRANCH} PARENT_SCOPE )
    SET( ${PROJ}_VERSION ${${PROJ}_VERSION} PARENT_SCOPE )

    # Save the version info
    SAVE_VERSION_INFO()

    # Load the version info (should already exist in install folder
    INCLUDE( "${${PROJ}_INSTALL_DIR}/${PROJ}_Version.cmake" )
    MESSAGE( "${PROJ} Version = ${${PROJ}_MAJOR_VERSION}.${${PROJ}_MINOR_VERSION}.${${PROJ}_BUILD_VERSION}" )

    # Write the version info to the file

    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.h" )
    STRING( REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}" )
    FILE( WRITE "${tmp_file}" "#ifndef ${PROJ}_VERSION_INCLUDE\n#define ${PROJ}_VERSION_INCLUDE\n\n" )
    FILE( APPEND "${tmp_file}" "#define ${namespace}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION}\n" )
    FILE( APPEND "${tmp_file}" "#define ${namespace}_MINOR_VERSION ${${PROJ}_MINOR_VERSION}\n" )
    FILE( APPEND "${tmp_file}" "#define ${namespace}_BUILD_VERSION ${${PROJ}_BUILD_VERSION}\n\n" )
    FILE( APPEND "${tmp_file}" "namespace ${namespace}::Version{\n\n" )
    FILE( APPEND "${tmp_file}" "static const int major = ${${PROJ}_MAJOR_VERSION};\n" )
    FILE( APPEND "${tmp_file}" "static const int minor = ${${PROJ}_MINOR_VERSION};\n" )
    FILE( APPEND "${tmp_file}" "static const int build = ${${PROJ}_BUILD_VERSION};\n\n" )
    FILE( APPEND "${tmp_file}" "static const char short_hash[] = \"${${PROJ}_SHORT_HASH_VERSION}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char long_hash[] = \"${${PROJ}_LONG_HASH_VERSION}\";\n\n" )

    # Write the compiler/flags
    STRING( REGEX REPLACE ";" " " C_FLAGS "${CMAKE_C_FLAGS}" )
    STRING( REGEX REPLACE ";" " " CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
    STRING( REGEX REPLACE ";" " " Fortran_FLAGS "${CMAKE_Fortran_FLAGS}" )
    FILE( APPEND "${tmp_file}" "static const char C[] = \"${CMAKE_C_COMPILER}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char CXX[] = \"${CMAKE_CXX_COMPILER}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char Fortran[] = \"${CMAKE_Fortran_COMPILER}\";\n\n" )
    FILE( APPEND "${tmp_file}" "static const char C_FLAGS[] = \"${C_FLAGS}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char CXX_FLAGS[] = \"${CXX_FLAGS}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char Fortran_FLAGS[] = \"${Fortran_FLAGS}\";\n\n" )
    FILE( APPEND "${tmp_file}" "static const char C_ID[] = \"${CMAKE_C_COMPILER_ID}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char CXX_ID[] = \"${CMAKE_CXX_COMPILER_ID}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char Fortran_ID[] = \"${CMAKE_Fortran_COMPILER_ID}\";\n\n" )
    FILE( APPEND "${tmp_file}" "static const char C_VERSION[] = \"${CMAKE_C_COMPILER_VERSION}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char CXX_VERSION[] = \"${CMAKE_CXX_COMPILER_VERSION}\";\n" )
    FILE( APPEND "${tmp_file}" "static const char Fortran_VERSION[] = \"${CMAKE_Fortran_COMPILER_VERSION}\";\n\n" )

    # Optional write of all changesets
    IF ( ${PROJ}_REPO_VERSION_REV )
        FILE( APPEND "${tmp_file}" "static const int ${PROJ}_REV[] = { ${PROJ}_REPO_VERSION_REV };\n\n" )
        FILE( APPEND "${tmp_file}" "static const char *${PROJ}_id[] = { ${PROJ}_REPO_VERSION_NODE };\n\n" )
    ENDIF()

    # Close the file
    FILE( APPEND "${tmp_file}" "}\n\n#endif\n" )

    # Copy the file only if it is different (to avoid rebuilding project)
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )

ENDFUNCTION()

FUNCTION( CREATE_RELEASE )
    ADD_CUSTOM_TARGET( release )
    INCLUDE( "${${PROJ}_INSTALL_DIR}/${PROJ}_Version.cmake" )
    SET( PROJ_NAME ${PROJ}-${${PROJ}_MAJOR_VERSION}.${${PROJ}_MINOR_VERSION}.${${PROJ}_BUILD_VERSION} )
    SET( RELEASE_FILENAME ${PROJ_NAME}.tar )
    SET( TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp" )
    FILE( WRITE "${TMP_DIR}/release.cmake" "# Create the release tar\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory \"${CMAKE_CURRENT_SOURCE_DIR}\" ${PROJ_NAME})\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJ_NAME}/.hg )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.hgtags )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.hgignore )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJ_NAME}/.git )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.gitlab-ci.yml )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-format )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-format-ignore )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-tidy )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy version.cmake ${PROJ_NAME}/${PROJ}_Version.cmake )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E tar -cfz ${PROJ_NAME}.tar.gz ${PROJ_NAME} )\n" )
    FILE( APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJ_NAME}.tar.gz \"${${PROJ}_INSTALL_DIR}/${${PROJ_NAME}}\" )\n" )
    ADD_CUSTOM_COMMAND( TARGET release PRE_BUILD COMMAND ${CMAKE_COMMAND} -P "${TMP_DIR}/release.cmake" WORKING_DIRECTORY "${TMP_DIR}" )
ENDFUNCTION()

# Write a cmake version file to the install directory
FUNCTION( SAVE_VERSION_INFO )

    # Set the major/minor versions if they are not set
    IF ( NOT ${PROJ}_MAJOR_VERSION )
        SET( ${PROJ}_MAJOR_VERSION 0 )
    ENDIF()
    IF ( NOT ${PROJ}_MINOR_VERSION )
        SET( ${PROJ}_MINOR_VERSION 0 )
    ENDIF()

    # Set the output filename
    SET( filename "${${PROJ}_INSTALL_DIR}/${PROJ}_Version.cmake" )

    # Write version info
    STRING( REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}" )
    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.cmake" )
    FILE( WRITE "${tmp_file}" "SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} )\n" )
    FILE( APPEND "${tmp_file}" "SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} )\n" )
    FILE( APPEND "${tmp_file}" "SET( ${PROJ}_BUILD_VERSION ${${PROJ}_BUILD_VERSION} )\n" )
    FILE( APPEND "${tmp_file}" "SET( ${PROJ}_SHORT_HASH_VERSION \"${${PROJ}_SHORT_HASH}\" )\n" )
    FILE( APPEND "${tmp_file}" "SET( ${PROJ}_LONG_HASH_VERSION  \"${${PROJ}_LONG_HASH}\" )\n" )
    FILE( APPEND "${tmp_file}" "SET( ${PROJ}_BRANCH  \"${${PROJ}_BRANCH}\" )\n" )

    # Copy the file only if it is different (to avoid rebuilding project)
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )

ENDFUNCTION()

# Get version info
FUNCTION( GET_VERSION_INFO PROJ SOURCE_DIR )

    # Set the major/minor versions if they are not set
    IF ( NOT ${PROJ}_MAJOR_VERSION )
        SET( ${PROJ}_MAJOR_VERSION 0 )
    ENDIF()
    IF ( NOT ${PROJ}_MINOR_VERSION )
        SET( ${PROJ}_MINOR_VERSION 0 )
    ENDIF()
    SET( ${PROJ}_REVISION 0 )

    # Get default version info from source version file (if it exists)
    IF ( EXISTS "${SOURCE_DIR}/${PROJ}_Version.cmake" )
        INCLUDE( "${SOURCE_DIR}/${PROJ}_Version.cmake" )
    ENDIF()

    # Get version info from mercurial
    GET_HG_INFO( ${PROJ} ${SOURCE_DIR} )

    # Get version info from git
    GET_GIT_INFO( ${PROJ} ${SOURCE_DIR} )

    # Get the build version
    SET( ${PROJ}_BUILD_VERSION ${${PROJ}_REVISION} )

    # Save the version info
    SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_BUILD_VERSION ${${PROJ}_BUILD_VERSION} PARENT_SCOPE )
    SET( ${PROJ}_SHORT_HASH ${${PROJ}_SHORT_HASH} PARENT_SCOPE )
    SET( ${PROJ}_LONG_HASH ${${PROJ}_LONG_HASH} PARENT_SCOPE )
    SET( ${PROJ}_BRANCH ${${PROJ}_BRANCH} PARENT_SCOPE )
    SET( ${PROJ}_VERSION "${${PROJ}_MAJOR_VERSION}.${${PROJ}_MINOR_VERSION}.${${PROJ}_BUILD_VERSION}" PARENT_SCOPE )
ENDFUNCTION()

# Get the repo version for mecurial
FUNCTION( GET_HG_INFO PROJ SOURCE_DIR )

    EXECUTE_PROCESS( COMMAND hg head WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE HG_INFO ERROR_VARIABLE HG_ERR )
    IF ( NOT ( "${HG_INFO}" MATCHES "changeset" ) )
        RETURN()
    ENDIF()

    EXECUTE_PROCESS( COMMAND hg id -i WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE VERSION_OUT )
    EXECUTE_PROCESS( COMMAND hg log --limit 1 --template "{rev};{node}" WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE VERSION_REV_OUT )
    EXECUTE_PROCESS( COMMAND hg identify -b WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE branch )

    STRING( REGEX REPLACE "(\r?\n)+$" "" short_hash "${VERSION_OUT}" )
    LIST( GET VERSION_REV_OUT 0 rev )
    LIST( GET VERSION_REV_OUT 1 long_hash )

    SET( ${PROJ}_REVISION ${rev} PARENT_SCOPE )
    SET( ${PROJ}_SHORT_HASH ${short_hash} PARENT_SCOPE )
    SET( ${PROJ}_LONG_HASH ${long_hash} PARENT_SCOPE )
    SET( ${PROJ}_BRANCH ${branch} PARENT_SCOPE )

ENDFUNCTION()

# Get the repo version for git
FUNCTION( GET_GIT_INFO PROJ SOURCE_DIR )
    EXECUTE_PROCESS( COMMAND git log -n 1 WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE GIT_INFO ERROR_VARIABLE GIT_ERR )
    IF ( NOT ( "${GIT_INFO}" MATCHES "commit " ) )
        RETURN()
    ENDIF()

    EXECUTE_PROCESS( COMMAND git rev-list --count HEAD WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE rev )
    EXECUTE_PROCESS( COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE short_hash )
    EXECUTE_PROCESS( COMMAND git rev-parse HEAD WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE long_hash )
    EXECUTE_PROCESS( COMMAND git rev-parse --abbrev-ref HEAD WORKING_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE branch )

    STRING( REGEX REPLACE "(\r?\n)+$" "" rev "${rev}" )
    STRING( REGEX REPLACE "(\r?\n)+$" "" short_hash "${short_hash}" )
    STRING( REGEX REPLACE "(\r?\n)+$" "" long_hash "${long_hash}" )
    STRING( REGEX REPLACE "(\r?\n)+$" "" branch "${branch}" )
    SET( ${PROJ}_REVISION ${rev} PARENT_SCOPE )
    SET( ${PROJ}_SHORT_HASH ${short_hash} PARENT_SCOPE )
    SET( ${PROJ}_LONG_HASH ${long_hash} PARENT_SCOPE )
    SET( ${PROJ}_BRANCH ${branch} PARENT_SCOPE )

ENDFUNCTION()
