set(ROBLOX_MODLOADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ROBLOX_MODLOADER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(MODLOADER_SOURCES
        "${ROBLOX_MODLOADER_SOURCE_DIR}/main.cpp"
        "${ROBLOX_MODLOADER_SOURCE_DIR}/pointers.cpp"
)

file(GLOB_RECURSE MODLOADER_SUBDIRS CONFIGURE_DEPENDS
        "${ROBLOX_MODLOADER_SOURCE_DIR}/**/*.cpp"
        "${ROBLOX_MODLOADER_SOURCE_DIR}/**/*.hpp"
        "${ROBLOX_MODLOADER_SOURCE_DIR}/**/*.mm"
)

if (NOT RML_ENABLE_LUAU)
    list(FILTER MODLOADER_SUBDIRS EXCLUDE REGEX "/luau/")
    list(FILTER MODLOADER_SUBDIRS EXCLUDE REGEX "/hooks/script_context/")
    list(FILTER MODLOADER_SUBDIRS EXCLUDE REGEX "/roblox/jobs/scripting/")
endif ()

foreach (platform_dir "windows" "macos" "linux")
    if (NOT platform_dir STREQUAL RML_PLATFORM_ID)
        list(FILTER MODLOADER_SUBDIRS EXCLUDE REGEX "/platform/${platform_dir}/")
    endif ()
endforeach ()

list(APPEND MODLOADER_SOURCES ${MODLOADER_SUBDIRS})

file(GLOB_RECURSE MODLOADER_VENDOR_SOURCES CONFIGURE_DEPENDS
        "${RML_THIRDPARTY_G3D_SRC_DIR}/*.cpp"
)

list(APPEND MODLOADER_SOURCES ${MODLOADER_VENDOR_SOURCES})

set(RML_VERSION_SCRIPT "${PROJECT_SOURCE_DIR}/cmake/generate-version.cmake")
set(RML_VERSION_INPUT "${ROBLOX_MODLOADER_SOURCE_DIR}/version.cpp.in")
set(RML_VERSION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/version.cpp")

find_package(Git QUIET)

execute_process(COMMAND ${CMAKE_COMMAND}
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DVERSION_REPO_DIR=${ROBLOX_MODLOADER_ROOT_DIR}"
        "-DVERSION_INPUT=${RML_VERSION_INPUT}"
        "-DVERSION_OUTPUT=${RML_VERSION_OUTPUT}"
        -P "${RML_VERSION_SCRIPT}"
)
set_source_files_properties("${RML_VERSION_OUTPUT}" PROPERTIES GENERATED TRUE)
list(APPEND MODLOADER_SOURCES "${RML_VERSION_OUTPUT}")

add_custom_target(roblox_modloader_version
        COMMAND ${CMAKE_COMMAND}
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DVERSION_REPO_DIR=${ROBLOX_MODLOADER_ROOT_DIR}"
        "-DVERSION_INPUT=${RML_VERSION_INPUT}"
        "-DVERSION_OUTPUT=${RML_VERSION_OUTPUT}"
        -P "${RML_VERSION_SCRIPT}"
        BYPRODUCTS "${RML_VERSION_OUTPUT}"
        COMMENT "Updating version information from git"
        VERBATIM
)

add_library(roblox_modloader SHARED ${MODLOADER_SOURCES})

add_dependencies(roblox_modloader roblox_modloader_version)

setup_compiler_flags(roblox_modloader)
setup_compile_definitions(roblox_modloader PRIVATE)
target_compile_definitions(roblox_modloader PRIVATE RML_ENABLE_LUAU=$<BOOL:${RML_ENABLE_LUAU}>)
target_compile_definitions(roblox_modloader PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
setup_core_dependencies(roblox_modloader PRIVATE)
setup_luau_dependencies(roblox_modloader PRIVATE)
setup_dotnet_hosting(roblox_modloader PRIVATE)

target_link_libraries(roblox_modloader PRIVATE rml_thirdparty)

if (APPLE)
    set(MODLOADER_OBJCXX_SOURCES ${MODLOADER_SOURCES})
    list(FILTER MODLOADER_OBJCXX_SOURCES INCLUDE REGEX "\\.mm$")
    set_source_files_properties(${MODLOADER_OBJCXX_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(roblox_modloader PRIVATE "-framework Foundation" "-framework Metal" "-framework QuartzCore")
endif ()

if (UNIX AND NOT APPLE)
    target_link_libraries(roblox_modloader PRIVATE dl)
endif ()

target_include_directories(roblox_modloader PRIVATE
        "${ROBLOX_MODLOADER_SOURCE_DIR}"
        "${ROBLOX_MODLOADER_INCLUDE_DIR}"
        "${ROBLOX_MODLOADER_SOURCE_DIR}/platform/${RML_PLATFORM_ID}"
)

set_target_properties(roblox_modloader PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        FOLDER "ModLoader"
        # Disable treating warnings as errors to avoid MSVC C2220 for third-party headers
        COMPILE_WARNING_AS_ERROR OFF
        ENABLE_EXPORTS ON
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_precompile_headers(roblox_modloader PRIVATE
        "${ROBLOX_MODLOADER_INCLUDE_DIR}/RobloxModLoader/internal/common.hpp"
)

include(GenerateExportHeader)
generate_export_header(roblox_modloader
        EXPORT_FILE_NAME "${ROBLOX_MODLOADER_INCLUDE_DIR}/RobloxModLoader/rml_export.hpp"
        EXPORT_MACRO_NAME "RML_EXPORT"
        NO_EXPORT_MACRO_NAME "RML_NO_EXPORT"
        DEPRECATED_MACRO_NAME "RML_DEPRECATED"
)

add_library(RobloxModLoader INTERFACE)
add_library(RobloxModLoader::RobloxModLoader ALIAS RobloxModLoader)

target_include_directories(RobloxModLoader INTERFACE
        "$<BUILD_INTERFACE:${ROBLOX_MODLOADER_INCLUDE_DIR}>"
        "$<INSTALL_INTERFACE:include>"
        "$<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${tomlplusplus_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${LUAU_SOURCE_DIR}/Compiler/include>"
        "$<BUILD_INTERFACE:${LUAU_SOURCE_DIR}/Ast/include>"
        "$<BUILD_INTERFACE:${LUAU_SOURCE_DIR}/VM/include>"
        "$<BUILD_INTERFACE:${LUAU_SOURCE_DIR}/Common/include>"
)

target_include_directories(RobloxModLoader SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${ROBLOX_MODLOADER_ROOT_DIR}/libs/third_party/g3d/include>"
        "$<BUILD_INTERFACE:${ROBLOX_MODLOADER_ROOT_DIR}/libs/third_party/bullet/include>"
        "$<BUILD_INTERFACE:${ROBLOX_MODLOADER_ROOT_DIR}/libs/third_party/rbxg3d/include>"
)

target_link_libraries(RobloxModLoader INTERFACE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
        Luau.Compiler
        Luau.Ast
        $<$<NOT:$<BOOL:${RML_ENABLE_LUAU}>>:Luau.VM>
        $<$<NOT:$<BOOL:${RML_ENABLE_LUAU}>>:Luau.VM.Internals>
        $<$<NOT:$<BOOL:${RML_ENABLE_LUAU}>>:Luau.CodeGen>
        $<$<TARGET_EXISTS:roblox_modloader>:roblox_modloader>
)

if (RML_PLATFORM_WINDOWS)
    target_include_directories(RobloxModLoader INTERFACE
            "$<BUILD_INTERFACE:${minhook_SOURCE_DIR}/include>"
            "$<BUILD_INTERFACE:${polyhook2_SOURCE_DIR}>"
    )

    target_link_libraries(RobloxModLoader INTERFACE
            minhook
            PolyHook_2
    )
endif ()

target_compile_features(RobloxModLoader INTERFACE cxx_std_23)

if (MSVC)
    target_compile_definitions(roblox_modloader PRIVATE "LUA_API=extern __declspec(dllexport)")
    target_compile_definitions(RobloxModLoader INTERFACE "LUA_API=extern __declspec(dllimport)")
endif ()

target_compile_definitions(RobloxModLoader INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        IS_RML=1
        TOML_EXCEPTIONS=0
        $<$<CONFIG:Debug>:DEBUG>
)

if (NOT RML_PLATFORM_WINDOWS)
    target_compile_definitions(RobloxModLoader INTERFACE SPDLOG_USE_STD_FORMAT)
endif ()

if (MSVC)
    target_compile_definitions(RobloxModLoader INTERFACE
            _ITERATOR_DEBUG_LEVEL=0
            _HAS_ITERATOR_DEBUGGING=0
    )
endif ()

setup_dotnet_hosting(RobloxModLoader INTERFACE)

set(RML_LUA_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/luau")

if (ROBLOX_MODLOADER_INSTALL)
    include(GNUInstallDirs)
    install(DIRECTORY "${RML_LUA_LIBS_DIR}/"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/RobloxModLoader/libraries
            FILES_MATCHING PATTERN "*.luau"
    )
endif ()

if (WIN32 AND TARGET roblox_modloader AND DEFINED ENV{ROBLOX_STUDIO_PATH} AND NOT "$ENV{ROBLOX_STUDIO_PATH}" STREQUAL "")
    if (EXISTS "${RML_LUA_LIBS_DIR}")
        add_custom_command(TARGET roblox_modloader POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/libraries"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RML_LUA_LIBS_DIR}"
                "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/libraries"
                COMMENT "Copying runtime Luau libraries to RobloxModLoader/libraries"
                VERBATIM
        )
    endif ()

    if (RML_DOTNET_RUNTIME_ROOT AND EXISTS "${RML_DOTNET_RUNTIME_ROOT}")
        add_custom_command(TARGET roblox_modloader POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RML_DOTNET_RUNTIME_ROOT}"
                "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime/dotnet"
                COMMENT "Copying bundled .NET runtime to RobloxModLoader/runtime/dotnet"
        )
    endif ()

    if (RML_DOTNET_GLOBAL_RUNTIME_CONFIG AND EXISTS "${RML_DOTNET_GLOBAL_RUNTIME_CONFIG}")
        add_custom_command(TARGET roblox_modloader POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RML_DOTNET_GLOBAL_RUNTIME_CONFIG}"
                "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime/RobloxModLoader.runtimeconfig.json"
                COMMENT "Copying global .NET runtimeconfig to RobloxModLoader/runtime"
        )
    endif ()

    if (RML_DOTNET_NETHOST_RUNTIME_BINARY AND EXISTS "${RML_DOTNET_NETHOST_RUNTIME_BINARY}")
        get_filename_component(RML_NETHOST_RUNTIME_NAME "${RML_DOTNET_NETHOST_RUNTIME_BINARY}" NAME)
        add_custom_command(TARGET roblox_modloader POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RML_DOTNET_NETHOST_RUNTIME_BINARY}"
                "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/runtime/${RML_NETHOST_RUNTIME_NAME}"
                COMMENT "Copying nethost runtime binary to RobloxModLoader/runtime"
        )
    endif ()

    add_custom_command(TARGET roblox_modloader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:roblox_modloader>
            "$ENV{ROBLOX_STUDIO_PATH}/$<TARGET_FILE_NAME:roblox_modloader>"
            COMMENT "Copying built roblox_modloader DLL to RobloxModLoader root"
            VERBATIM
    )
endif ()

if (APPLE AND TARGET roblox_modloader AND DEFINED ENV{ROBLOX_STUDIO_PATH} AND NOT "$ENV{ROBLOX_STUDIO_PATH}" STREQUAL "")
    set(_rml_studio "$ENV{ROBLOX_STUDIO_PATH}")
    if (_rml_studio MATCHES "Contents/MacOS/?$")
        set(_rml_macos_dir "${_rml_studio}")
    else ()
        set(_rml_macos_dir "${_rml_studio}/Contents/MacOS")
    endif ()

    set(_rml_deployed_dylib "${_rml_macos_dir}/roblox_modloader.dylib")

    add_custom_command(TARGET roblox_modloader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_rml_macos_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:roblox_modloader>
            "${_rml_deployed_dylib}"
            COMMAND codesign --force --sign - "${_rml_deployed_dylib}"
            COMMENT "Deploying roblox_modloader.dylib into Studio (Contents/MacOS) and ad-hoc signing it"
            VERBATIM
    )

    unset(_rml_studio)
    unset(_rml_macos_dir)
    unset(_rml_deployed_dylib)
endif ()

if (RML_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
