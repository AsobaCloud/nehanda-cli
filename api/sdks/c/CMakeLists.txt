cmake_minimum_required (VERSION 2.6...3.10.2)
project (CGenerator C)

cmake_policy(SET CMP0063 NEW)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-function-declaration")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=missing-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=int-conversion")

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

find_package(OpenSSL)

if (OPENSSL_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DOPENSSL")
    if(CMAKE_VERSION VERSION_LESS 3.4)
        include_directories(${OPENSSL_INCLUDE_DIR})
        include_directories(${OPENSSL_INCLUDE_DIRS})
        link_directories(${OPENSSL_LIBRARIES})
    endif()
endif()

set(pkgName "aimee_kb_api")

# this default version can be overridden in PreTarget.cmake
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 1)

if( (DEFINED CURL_INCLUDE_DIR) AND (DEFINED CURL_LIBRARIES))
    include_directories(${CURL_INCLUDE_DIR})
    set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} ${CURL_LIBRARIES} )
else()
    find_package(CURL 7.58.0 REQUIRED)
    if(CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIR})
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} ${CURL_LIBRARIES} )
    endif()
endif()

set(SRCS
    src/list.c
    src/apiKey.c
    src/apiClient.c
    src/binary.c
    external/cJSON.c
    model/object.c
    model/active_release_response.c
    model/artifact_links_response.c
    model/artifact_links_response_links_inner.c
    model/artifact_response.c
    model/blast_radius_response.c
    model/capabilities_response.c
    model/code_build_request.c
    model/code_build_response.c
    model/code_caller_hit.c
    model/code_callers_response.c
    model/code_definition.c
    model/code_find_hit.c
    model/code_find_response.c
    model/code_project.c
    model/code_project_language.c
    model/code_project_stats_response.c
    model/code_projects_response.c
    model/code_scan_request.c
    model/code_scan_response.c
    model/code_search_hit.c
    model/code_search_response.c
    model/code_structure_response.c
    model/code_update_request.c
    model/code_update_response.c
    model/create_release_request.c
    model/create_release_response.c
    model/doc_ingest_response.c
    model/doc_metadata_response.c
    model/docs_manifest_request.c
    model/docs_manifest_request_docs_inner.c
    model/docs_manifest_response.c
    model/docs_manifest_response_missing_inner.c
    model/drain_request.c
    model/drain_response.c
    model/entity_profile_response.c
    model/entity_search_request.c
    model/entity_search_response.c
    model/entity_search_response_entities_inner.c
    model/error_response.c
    model/health_response.c
    model/ingest_request.c
    model/ingest_response.c
    model/ingest_status_response.c
    model/ingest_status_response_queue.c
    model/ingest_status_response_recent_inner.c
    model/ingest_status_response_workers.c
    model/job_status_response.c
    model/maintenance_clear_request.c
    model/maintenance_clear_response.c
    model/maintenance_reconcile_request.c
    model/maintenance_reconcile_response.c
    model/maintenance_reconcile_response_memory.c
    model/maintenance_repair_request.c
    model/maintenance_repair_response.c
    model/pipeline_status_response.c
    model/pipeline_status_response_queue.c
    model/review_accept_request.c
    model/review_queue_response.c
    model/review_reject_request.c
    model/rollback_request.c
    model/search_hit.c
    model/search_hit_citations_inner.c
    model/search_request.c
    model/search_response.c
    model/version_response.c
    model/workers_response.c
    api/DefaultAPI.c

)

set(HDRS
    include/apiClient.h
    include/list.h
    include/binary.h
    include/keyValuePair.h
    external/cJSON.h
    model/object.h
    model/any_type.h
    model/active_release_response.h
    model/artifact_links_response.h
    model/artifact_links_response_links_inner.h
    model/artifact_response.h
    model/blast_radius_response.h
    model/capabilities_response.h
    model/code_build_request.h
    model/code_build_response.h
    model/code_caller_hit.h
    model/code_callers_response.h
    model/code_definition.h
    model/code_find_hit.h
    model/code_find_response.h
    model/code_project.h
    model/code_project_language.h
    model/code_project_stats_response.h
    model/code_projects_response.h
    model/code_scan_request.h
    model/code_scan_response.h
    model/code_search_hit.h
    model/code_search_response.h
    model/code_structure_response.h
    model/code_update_request.h
    model/code_update_response.h
    model/create_release_request.h
    model/create_release_response.h
    model/doc_ingest_response.h
    model/doc_metadata_response.h
    model/docs_manifest_request.h
    model/docs_manifest_request_docs_inner.h
    model/docs_manifest_response.h
    model/docs_manifest_response_missing_inner.h
    model/drain_request.h
    model/drain_response.h
    model/entity_profile_response.h
    model/entity_search_request.h
    model/entity_search_response.h
    model/entity_search_response_entities_inner.h
    model/error_response.h
    model/health_response.h
    model/ingest_request.h
    model/ingest_response.h
    model/ingest_status_response.h
    model/ingest_status_response_queue.h
    model/ingest_status_response_recent_inner.h
    model/ingest_status_response_workers.h
    model/job_status_response.h
    model/maintenance_clear_request.h
    model/maintenance_clear_response.h
    model/maintenance_reconcile_request.h
    model/maintenance_reconcile_response.h
    model/maintenance_reconcile_response_memory.h
    model/maintenance_repair_request.h
    model/maintenance_repair_response.h
    model/pipeline_status_response.h
    model/pipeline_status_response_queue.h
    model/review_accept_request.h
    model/review_queue_response.h
    model/review_reject_request.h
    model/rollback_request.h
    model/search_hit.h
    model/search_hit_citations_inner.h
    model/search_request.h
    model/search_response.h
    model/version_response.h
    model/workers_response.h
    api/DefaultAPI.h

)

include(PreTarget.cmake OPTIONAL)

set(PROJECT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Add library with project file with project name as library name
if(NOT BUILD_STATIC_LIBS)
  add_library(${pkgName} ${SRCS} ${HDRS})
else()
  add_library(${pkgName} STATIC ${SRCS} ${HDRS})
endif()
# Link dependent libraries
if(NOT CMAKE_VERSION VERSION_LESS 3.4)
    target_link_libraries(${pkgName} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
target_link_libraries(${pkgName} PUBLIC ${CURL_LIBRARIES} )
target_include_directories(
    ${pkgName} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

include(PostTarget.cmake OPTIONAL)

# installation of libraries, headers, and config files
if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in)
    install(TARGETS ${pkgName} DESTINATION lib)
else()
    include(GNUInstallDirs)
    install(TARGETS ${pkgName} DESTINATION lib EXPORT ${pkgName}Targets)

    foreach(HDR_FILE ${HDRS})
        get_filename_component(HDR_DIRECTORY ${HDR_FILE} DIRECTORY)
        get_filename_component(ABSOLUTE_HDR_DIRECTORY ${HDR_DIRECTORY} ABSOLUTE)
        file(RELATIVE_PATH RELATIVE_HDR_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${ABSOLUTE_HDR_DIRECTORY})
        install(FILES ${HDR_FILE} DESTINATION include/${pkgName}/${RELATIVE_HDR_PATH})
    endforeach()

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${pkgName}/${pkgName}ConfigVersion.cmake"
            VERSION "${PROJECT_VERSION_STRING}"
            COMPATIBILITY AnyNewerVersion
    )

    export(EXPORT ${pkgName}Targets
            FILE "${CMAKE_CURRENT_BINARY_DIR}/${pkgName}/${pkgName}Targets.cmake"
            NAMESPACE ${pkgName}::
            )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/${pkgName}/${pkgName}Config.cmake"
            @ONLY
            )

    set(ConfigPackageLocation lib/cmake/${pkgName})
    install(EXPORT ${pkgName}Targets
            FILE
            ${pkgName}Targets.cmake
            NAMESPACE
            ${pkgName}::
            DESTINATION
            ${ConfigPackageLocation}
            )
    install(
            FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${pkgName}/${pkgName}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${pkgName}/${pkgName}ConfigVersion.cmake"
            DESTINATION
            ${ConfigPackageLocation}
    )
endif()

# make installation packages
include(Packing.cmake OPTIONAL)

# Setting file variables to null
set(SRCS "")
set(HDRS "")


## This section shows how to use the above compiled library to compile the source files
## set source files
#set(SRCS
#    unit-tests/manual-DefaultAPI.c
#)

##set header files
#set(HDRS
#)

## loop over all files in SRCS variable
#foreach(SOURCE_FILE ${SRCS})
#    # Get only the file name from the file as add_executable does not support executable with slash("/")
#    get_filename_component(FILE_NAME_ONLY ${SOURCE_FILE} NAME_WE)
#    # Remove .c from the file name and set it as executable name
#    string( REPLACE ".c" "" EXECUTABLE_FILE ${FILE_NAME_ONLY})
#    # Add executable for every source file in  SRCS
#    add_executable(unit-${EXECUTABLE_FILE} ${SOURCE_FILE})
#    # Link above created library to executable and dependent library curl
#    target_link_libraries(unit-${EXECUTABLE_FILE} ${CURL_LIBRARIES} ${pkgName} )
#endforeach(SOURCE_FILE ${SRCS})
