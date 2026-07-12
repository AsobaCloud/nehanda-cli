cmake_minimum_required(VERSION 3.16)
project(nehanda-cli C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(NEHANDA_BUILD=1)

if(NOT DEFINED NEHANDA_VERSION_STR)
  execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --always --dirty
    OUTPUT_VARIABLE NEHANDA_VERSION_STR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT NEHANDA_VERSION_STR)
    set(NEHANDA_VERSION_STR "dev")
  endif()
endif()
add_compile_definitions(NEHANDA_VERSION_STR="${NEHANDA_VERSION_STR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(OPENSSL REQUIRED openssl)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/upstream/src
    ${CMAKE_CURRENT_SOURCE_DIR}/upstream/src/headers
    ${SQLITE3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

set(NEHANDA_SOURCES
    src/nehanda_config.c
    src/nehanda_auth.c
    src/nehanda_header_inject.c
    src/nehanda_unified_storage.c
    src/nehanda_inprocess.c
    src/nehanda_token.c
    upstream/src/acp_server.c
    upstream/src/cli_main.c
    upstream/src/aimee_client.c
    upstream/src/aimee_home.c
    upstream/src/aimee_tls.c
    upstream/src/conversation.c
    upstream/src/conversation_context.c
    upstream/src/compact.c
    upstream/src/config.c
    upstream/src/dstr.c
    upstream/src/events.c
    upstream/src/diff.c
    upstream/src/db1/db.c
    upstream/src/db1/db_schema.c
    upstream/src/db1/runtime_state.c
    upstream/src/db1/session_state.c
    upstream/src/db1/user_memory.c
    upstream/src/db1/server_sessions.c
    upstream/src/db1/local_operator.c
    upstream/src/db2/entity_nodes.c
    upstream/src/db2/entity_edges.c
    upstream/src/db2/kb_docs.c
    upstream/src/db2/memory_vectors.c
    upstream/src/db2/canonical_index.c
    upstream/src/db2/code_index.c
    upstream/src/db2/code_projection.c
    upstream/src/db2/stopwords.c
)

add_executable(nehanda ${NEHANDA_SOURCES})

target_link_libraries(nehanda
    PRIVATE
    ${SQLITE3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    m
)

install(TARGETS nehanda DESTINATION bin)
