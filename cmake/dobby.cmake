include(cmake/CPM.cmake)

CPMAddPackage(
        NAME dobby
        GITHUB_REPOSITORY jmpews/Dobby
        GIT_TAG 5dfc8546954ce3b3198132ab13fddb89ee92cdd7
        PATCHES "${CMAKE_CURRENT_LIST_DIR}/patches/dobby-atomic-code-patch.patch"
        OPTIONS
        "DOBBY_DEBUG OFF"
        "DOBBY_BUILD_EXAMPLE OFF"
        "DOBBY_BUILD_TEST OFF"
        "Plugin.SymbolResolver ON"
        "Plugin.ImportTableReplace OFF"
)
