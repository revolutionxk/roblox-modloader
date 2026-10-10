CPMAddPackage(
        NAME tracy
        GITHUB_REPOSITORY revolutionxk/tracy
        GIT_TAG rml
        OPTIONS
        "TRACY_ENABLE ON"
        "TRACY_ON_DEMAND ON"
        "TRACY_MANUAL_LIFETIME ON"
        "TRACY_NO_CRASH_HANDLER ON"
        "TRACY_STATIC ON"
        "TRACY_ONLY_LOCALHOST ON"
)

target_compile_definitions(TracyClient PUBLIC TRACY_DELAYED_INIT)
set_target_properties(TracyClient PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")
