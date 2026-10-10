include(${CMAKE_SOURCE_DIR}/cmake/tracy.cmake)

file(GLOB TRACY_PROFILER_SOURCES CONFIGURE_DEPENDS "${CURRENT_MOD_DIR}/*.cpp" "${CURRENT_MOD_DIR}/*.hpp")

create_custom_roblox_mod(
        SOURCES ${TRACY_PROFILER_SOURCES}
        DEPENDENCIES TracyClient
        INCLUDE_DIRS "${CURRENT_MOD_DIR}/platform/${RML_PLATFORM_ID}"
)
