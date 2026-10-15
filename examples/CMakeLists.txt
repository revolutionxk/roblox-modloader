message(STATUS "Configuring RobloxModLoader examples")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(create_roblox_mod MOD_NAME MOD_DIR)
    message(STATUS "Creating mod: ${MOD_NAME}")

    set(MOD_CMAKE_FILE "${MOD_DIR}/CMakeLists.txt")
    if (EXISTS ${MOD_CMAKE_FILE})
        message(STATUS "Found custom CMakeLists.txt for ${MOD_NAME}, including it...")

        set(CURRENT_MOD_NAME ${MOD_NAME})
        set(CURRENT_MOD_DIR ${MOD_DIR})

        include(${MOD_CMAKE_FILE})

        if (TARGET ${MOD_NAME})
            message(STATUS "Target ${MOD_NAME} was created by custom CMakeLists.txt")
            return()
        else ()
            message(STATUS "Custom CMakeLists.txt didn't create target, proceeding with default creation...")
        endif ()
    endif ()

    file(GLOB_RECURSE MOD_SOURCES CONFIGURE_DEPENDS
            "${MOD_DIR}/*.cpp"
            "${MOD_DIR}/*.hpp"
            "${MOD_DIR}/*.h"
    )

    list(FILTER MOD_SOURCES EXCLUDE REGEX ".*CMakeLists\\.txt$")

    if (NOT MOD_SOURCES)
        message(WARNING "No source files found for mod ${MOD_NAME}")
        return()
    endif ()

    add_library(${MOD_NAME} MODULE ${MOD_SOURCES})

    target_link_libraries(${MOD_NAME} PRIVATE RobloxModLoader::RobloxModLoader)

    set_target_properties(${MOD_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            COMPILE_WARNING_AS_ERROR ON
            FOLDER "Examples"
    )

    if (MSVC)
        target_compile_options(${MOD_NAME} PRIVATE
                /bigobj
                /utf-8
                $<$<CONFIG:Debug>:/ZI>
                $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Ot /Oy /Ob3 /sdl- /GL /GF /GS- /Gw>
        )
        target_link_options(${MOD_NAME} PRIVATE
                $<$<CONFIG:Debug>:/INCREMENTAL>
                $<$<CONFIG:RelWithDebInfo>:/LTCG /OPT:REF,ICF /GUARD:NO>
        )
    endif ()

    source_group(TREE ${MOD_DIR} PREFIX "src" FILES ${MOD_SOURCES})

    message(STATUS "Created mod target: ${MOD_NAME}")
endfunction()

function(create_custom_roblox_mod)
    if (NOT DEFINED CURRENT_MOD_NAME OR NOT DEFINED CURRENT_MOD_DIR)
        message(FATAL_ERROR "create_custom_roblox_mod can only be called from within a mod's CMakeLists.txt")
    endif ()

    set(MOD_NAME ${CURRENT_MOD_NAME})
    set(MOD_DIR ${CURRENT_MOD_DIR})

    cmake_parse_arguments(MOD "" "" "SOURCES;DEPENDENCIES;INCLUDE_DIRS;COMPILE_DEFINITIONS;POST_BUILD_COMMANDS" ${ARGN})

    if (MOD_SOURCES)
        set(SOURCES ${MOD_SOURCES})
    else ()
        file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
                "${MOD_DIR}/*.cpp"
                "${MOD_DIR}/*.hpp"
                "${MOD_DIR}/*.h"
        )
        list(FILTER SOURCES EXCLUDE REGEX ".*CMakeLists\\.txt$")
    endif ()

    if (NOT SOURCES)
        message(WARNING "No source files found for custom mod ${MOD_NAME}")
        return()
    endif ()

    add_library(${MOD_NAME} MODULE ${SOURCES})

    target_link_libraries(${MOD_NAME} PRIVATE RobloxModLoader::RobloxModLoader)

    if (MOD_DEPENDENCIES)
        target_link_libraries(${MOD_NAME} PRIVATE ${MOD_DEPENDENCIES})
    endif ()

    if (MOD_INCLUDE_DIRS)
        target_include_directories(${MOD_NAME} PRIVATE ${MOD_INCLUDE_DIRS})
    endif ()

    if (MOD_COMPILE_DEFINITIONS)
        target_compile_definitions(${MOD_NAME} PRIVATE ${MOD_COMPILE_DEFINITIONS})
    endif ()

    set_target_properties(${MOD_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            COMPILE_WARNING_AS_ERROR ON
            FOLDER "Examples"
    )

    if (MSVC)
        target_compile_options(${MOD_NAME} PRIVATE
                /bigobj
                /utf-8
                $<$<CONFIG:Debug>:/ZI>
                $<$<CONFIG:RelWithDebInfo>:/O2 /Oi /Ot /Oy /Ob3 /sdl- /GL /GF /GS- /Gw>
        )
        target_link_options(${MOD_NAME} PRIVATE
                $<$<CONFIG:Debug>:/INCREMENTAL>
                $<$<CONFIG:RelWithDebInfo>:/LTCG /OPT:REF,ICF /GUARD:NO>
        )
    endif ()

    if (MOD_POST_BUILD_COMMANDS)
        foreach (COMMAND IN LISTS MOD_POST_BUILD_COMMANDS)
            add_custom_command(TARGET ${MOD_NAME} POST_BUILD COMMAND ${COMMAND})
        endforeach ()
    endif ()

    source_group(TREE ${MOD_DIR} PREFIX "src" FILES ${SOURCES})

    message(STATUS "Created custom mod target: ${MOD_NAME}")
endfunction()

file(GLOB EXAMPLE_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

foreach (EXAMPLE_DIR ${EXAMPLE_DIRS})
    if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR} AND
    (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/main.cpp OR
            EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/CMakeLists.txt))

        if (NOT RML_ENABLE_LUAU AND EXAMPLE_DIR MATCHES "^(discord_rpc|studio_theme)$")
            message(STATUS "Skipping ${EXAMPLE_DIR} (requires RML_ENABLE_LUAU)")
        else ()
            create_roblox_mod(${EXAMPLE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR})
        endif ()
    else ()
        message(STATUS "Skipping ${EXAMPLE_DIR} (not a valid mod directory)")
    endif ()
endforeach ()

if (WIN32 AND DEFINED ENV{ROBLOX_STUDIO_PATH} AND NOT "$ENV{ROBLOX_STUDIO_PATH}" STREQUAL "")
    set(RML_SCRIPT_MOD_DIRS "")

    foreach (EXAMPLE_DIR ${EXAMPLE_DIRS})
        if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR} AND
        EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/mod.toml AND
        IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/scripts AND
        NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/main.cpp AND
        NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE_DIR}/CMakeLists.txt)
            list(APPEND RML_SCRIPT_MOD_DIRS ${EXAMPLE_DIR})
        endif ()
    endforeach ()

    if (RML_SCRIPT_MOD_DIRS)
        add_custom_target(rml_script_mods ALL)

        foreach (SCRIPT_MOD ${RML_SCRIPT_MOD_DIRS})
            set(RML_SCRIPT_MOD_TARGET_DIR "$ENV{ROBLOX_STUDIO_PATH}/RobloxModLoader/mods/${SCRIPT_MOD}")

            add_custom_command(TARGET rml_script_mods POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${RML_SCRIPT_MOD_TARGET_DIR}/scripts"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_MOD}/mod.toml"
                    "${RML_SCRIPT_MOD_TARGET_DIR}/mod.toml"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_MOD}/scripts"
                    "${RML_SCRIPT_MOD_TARGET_DIR}/scripts"
                    COMMENT "Deploying script mod ${SCRIPT_MOD}"
                    VERBATIM
            )
        endforeach ()

        message(STATUS "Script-only mods deployed on build: ${RML_SCRIPT_MOD_DIRS}")
    endif ()
endif ()
