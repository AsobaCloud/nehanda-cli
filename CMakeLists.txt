cmake_minimum_required(VERSION 3.16)
project(nehanda-cli C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ── Version ───────────────────────────────────────────────────────────────────
# Format: <nehanda-cli version>+aimee-<upstream commit>
# Pass -DNEHANDA_VERSION_STR=vX.Y.Z at release time.
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

# ── Upstream aimee build ──────────────────────────────────────────────────────
# Pull in all upstream targets. NEHANDA_BUILD=1 lets upstream code
# #ifdef around anything we override.
add_compile_definitions(NEHANDA_BUILD=1)
add_subdirectory(upstream)

# ── nehanda-cli additions ─────────────────────────────────────────────────────
add_library(nehanda_extensions STATIC
  src/nehanda_auth.c           # device-flow login, session token storage
  src/nehanda_config.c         # NEHANDA_API_KEY + gateway URL resolution
  src/nehanda_header_inject.c  # wraps upstream agent_http.c, injects auth header
)

target_include_directories(nehanda_extensions PRIVATE
  upstream/src
  upstream/src/headers
)

# Rename the upstream 'aimee' binary to 'nehanda' and link our extensions.
if(TARGET aimee)
  set_target_properties(aimee PROPERTIES OUTPUT_NAME "nehanda")
  target_link_libraries(aimee PRIVATE nehanda_extensions)
endif()

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS aimee DESTINATION bin)
install(FILES config/nehanda.yaml.example DESTINATION share/nehanda-cli)
