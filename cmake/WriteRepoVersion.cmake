FUNCTION( WRITE_REPO_VERSION )

    # Check the inputs
    IF ( NOT PROJ )
        MESSAGE( FATAL_ERROR "PROJ is not set")
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

    # Save the version info
    SAVE_VERSION_INFO( )

    # Load the version info (should already exist in install folder
    INCLUDE( "${${PROJ}_INSTALL_DIR}/${PROJ}_Version.cmake" )
    MESSAGE("${PROJ} Version = ${${PROJ}_MAJOR_VERSION}.${${PROJ}_MINOR_VERSION}.${${PROJ}_BUILD_VERSION}")

    # Write the version info to the file
    SET( filename "${${PROJ}_INSTALL_DIR}/include/${${PROJ}_INC}/${PROJ}_Version.h" )
    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.h" )
    STRING(REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}")
    FILE(WRITE  "${tmp_file}" "#ifndef ${PROJ}_VERSION_INCLUDE\n#define ${PROJ}_VERSION_INCLUDE\n\n" )
    FILE(APPEND "${tmp_file}" "namespace ${namespace}::Version{\n\n" )
    FILE(APPEND "${tmp_file}" "static const int major = ${${PROJ}_MAJOR_VERSION};\n" )
    FILE(APPEND "${tmp_file}" "static const int minor = ${${PROJ}_MINOR_VERSION};\n" )
    FILE(APPEND "${tmp_file}" "static const int build = ${${PROJ}_BUILD_VERSION};\n\n" )
    FILE(APPEND "${tmp_file}" "static const char short_hash[] = \"${${PROJ}_SHORT_HASH_VERSION}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char long_hash[] = \"${${PROJ}_LONG_HASH_VERSION}\";\n\n" )

    # Write the compiler/flags
    STRING(REGEX REPLACE ";" " " C_FLAGS "${CMAKE_C_FLAGS}")
    STRING(REGEX REPLACE ";" " " CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    STRING(REGEX REPLACE ";" " " Fortran_FLAGS "${CMAKE_Fortran_FLAGS}")
    FILE(APPEND "${tmp_file}" "static const char C[] = \"${CMAKE_C_COMPILER}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char CXX[] = \"${CMAKE_CXX_COMPILER}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char Fortran[] = \"${CMAKE_Fortran_COMPILER}\";\n\n" )
    FILE(APPEND "${tmp_file}" "static const char C_FLAGS[] = \"${C_FLAGS}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char CXX_FLAGS[] = \"${CXX_FLAGS}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char Fortran_FLAGS[] = \"${Fortran_FLAGS}\";\n\n" )
    FILE(APPEND "${tmp_file}" "static const char C_ID[] = \"${CMAKE_C_COMPILER_ID}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char CXX_ID[] = \"${CMAKE_CXX_COMPILER_ID}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char Fortran_ID[] = \"${CMAKE_Fortran_COMPILER_ID}\";\n\n" )
    FILE(APPEND "${tmp_file}" "static const char C_VERSION[] = \"${CMAKE_C_COMPILER_VERSION}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char CXX_VERSION[] = \"${CMAKE_CXX_COMPILER_VERSION}\";\n" )
    FILE(APPEND "${tmp_file}" "static const char Fortran_VERSION[] = \"${CMAKE_Fortran_COMPILER_VERSION}\";\n\n" )

    # Optional write of all changesets
    IF ( ${PROJ}_REPO_VERSION_REV )
        FILE(APPEND "${tmp_file}" "static const int ${PROJ}_REV[] = { ${PROJ}_REPO_VERSION_REV };\n\n" )
        FILE(APPEND "${tmp_file}" "static const char *${PROJ}_id[] = { ${PROJ}_REPO_VERSION_NODE };\n\n" )
    ENDIF()

    # Close the file
    FILE(APPEND "${tmp_file}" "}\n\n#endif\n" )

    # Copy the file only if it is different (to avoid rebuilding project)
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )

ENDFUNCTION()


FUNCTION( CREATE_RELEASE )
    ADD_CUSTOM_TARGET( release )
    INCLUDE( "${${PROJ}_INSTALL_DIR}/${PROJ}_Version.cmake" )
    SET( PROJ_NAME ${PROJ}-${${PROJ}_MAJOR_VERSION}.${${PROJ}_MINOR_VERSION}.${${PROJ}_BUILD_VERSION} )
    SET( RELEASE_FILENAME ${PROJ_NAME}.tar )
    SET( TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmp" )
    FILE(WRITE  "${TMP_DIR}/release.cmake" "# Create the release tar\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory \"${CMAKE_CURRENT_SOURCE_DIR}\" ${PROJ_NAME})\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJ_NAME}/.hg )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.hgtags )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.hgignore )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJ_NAME}/.git )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.gitlab-ci.yml )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-format )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-format-ignore )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJ_NAME}/.clang-tidy )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy version.cmake ${PROJ_NAME}/${PROJ}_Version.cmake )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E tar -cfz ${PROJ_NAME}.tar.gz ${PROJ_NAME} )\n" )
    FILE(APPEND "${TMP_DIR}/release.cmake" "execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${PROJ_NAME}.tar.gz \"${${PROJ}_INSTALL_DIR}/${${PROJ_NAME}}\" )\n" )
    ADD_CUSTOM_COMMAND( TARGET release PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -P "${TMP_DIR}/release.cmake"
        WORKING_DIRECTORY "${TMP_DIR}" 
    )
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

    # Check if a version file exists in the src tree
    IF ( EXISTS "${src_dir}/${PROJ}_Version.cmake" )
        EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src_dir}/${PROJ}_Version.cmake" "${filename}" )
        RETURN()
    ENDIF()

    # Get version info from mercurial
    EXECUTE_PROCESS( COMMAND hg head  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE HG_INFO ERROR_VARIABLE HG_ERR )
    IF ( "${HG_INFO}" MATCHES "changeset")
        WRITE_HG_INFO()
        RETURN()
    ENDIF()

    # Get version info from git
    EXECUTE_PROCESS( COMMAND git log -n 1  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE GIT_INFO ERROR_VARIABLE GIT_ERR )
    IF ( "${GIT_INFO}" MATCHES "commit ")
        WRITE_GIT_INFO()
        RETURN()
    ENDIF()

    # Write default info
    STRING(REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}")
    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.cmake" )
    FILE(WRITE  "${tmp_file}" "SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BUILD_VERSION 0 )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_SHORT_HASH_VERSION \"\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_LONG_HASH_VERSION  \"\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BRANCH  \"\" )\n" )

ENDFUNCTION()



FUNCTION( WRITE_HG_INFO )

    # Get the repo version
    EXECUTE_PROCESS( COMMAND hg id -i  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE VERSION_OUT )
    EXECUTE_PROCESS( COMMAND hg log --limit 1 --template "{rev};{node}"  WORKING_DIRECTORY "${src_dir}" OUTPUT_VARIABLE VERSION_REV_OUT  )
    EXECUTE_PROCESS( COMMAND hg identify -b  WORKING_DIRECTORY "${src_dir}" OUTPUT_VARIABLE branch  )

    STRING(REGEX REPLACE "(\r?\n)+$" "" short_hash "${VERSION_OUT}")
    LIST(GET VERSION_REV_OUT 0 rev )
    LIST(GET VERSION_REV_OUT 1 long_hash )

    # Write the results to the file
    STRING(REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}")
    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.cmake" )
    FILE(WRITE  "${tmp_file}" "SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BUILD_VERSION ${rev} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_SHORT_HASH_VERSION \"${short_hash}\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_LONG_HASH_VERSION  \"${long_hash}\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BRANCH  \"${branch}\" )\n" )

    # Optional write of all changesets
    IF ( WRITE_ALL_CHANGESETS )
        # Get all changesets
        EXECUTE_PROCESS( COMMAND hg log --template "{rev}\n"  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE VERSION_REV_OUT  )
        EXECUTE_PROCESS( COMMAND hg log --template "{node}\n" WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE VERSION_NODE_OUT )
        STRING(REGEX REPLACE "\n" ", " VERSION_REV_OUT "${VERSION_REV_OUT}")
        STRING(REGEX REPLACE "\n" "\", \"" VERSION_NODE_OUT "\"${VERSION_NODE_OUT}")
        STRING(REGEX REPLACE ", ;"   "" VERSION_REV_OUT  "${VERSION_REV_OUT};"  )
        STRING(REGEX REPLACE ", \";" "" VERSION_NODE_OUT "${VERSION_NODE_OUT};" )
        LIST( LENGTH VERSION_REV_OUT OUTPUT_LENGTH )
        # Write the results to the file
        FILE(APPEND "SET( ${PROJ}_REPO_VERSION_REV ${VERSION_REV_OUT} )\n" )
        FILE(APPEND "SET( ${PROJ}_REPO_VERSION_REV ${VERSION_NODE_OUT} )\n" )
    ENDIF()

    # Copy the file only if it is different (to avoid rebuilding project)
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )

ENDFUNCTION()


FUNCTION( WRITE_GIT_INFO )

    # Get the repo version
    EXECUTE_PROCESS( COMMAND git rev-list --count HEAD  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE rev )
    EXECUTE_PROCESS( COMMAND git rev-parse --short HEAD  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE short_hash )
    EXECUTE_PROCESS( COMMAND git rev-parse HEAD  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE long_hash )
    EXECUTE_PROCESS( COMMAND git rev-parse --abbrev-ref HEAD  WORKING_DIRECTORY "${src_dir}"  OUTPUT_VARIABLE branch )

    STRING(REGEX REPLACE "(\r?\n)+$" "" rev "${rev}")
    STRING(REGEX REPLACE "(\r?\n)+$" "" short_hash "${short_hash}")
    STRING(REGEX REPLACE "(\r?\n)+$" "" long_hash "${long_hash}")
    STRING(REGEX REPLACE "(\r?\n)+$" "" branch "${branch}")

    # Write the results to the file
    STRING(REGEX REPLACE " " "_" namespace "${${PROJ}_NAMESPACE}")
    SET( tmp_file "${CMAKE_CURRENT_BINARY_DIR}/tmp/version.cmake" )
    FILE(WRITE  "${tmp_file}" "SET( ${PROJ}_MAJOR_VERSION ${${PROJ}_MAJOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_MINOR_VERSION ${${PROJ}_MINOR_VERSION} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BUILD_VERSION ${rev} )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_SHORT_HASH_VERSION \"${short_hash}\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_LONG_HASH_VERSION  \"${long_hash}\" )\n" )
    FILE(APPEND "${tmp_file}" "SET( ${PROJ}_BRANCH  \"${branch}\" )\n" )

    # Copy the file only if it is different (to avoid rebuilding project)
    EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E copy_if_different "${tmp_file}" "${filename}" )

ENDFUNCTION()
