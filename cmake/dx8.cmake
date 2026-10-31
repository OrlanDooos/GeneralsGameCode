FetchContent_Declare(
    dx8
    GIT_REPOSITORY https://github.com/TheSuperHackers/min-dx8-sdk.git
    GIT_TAG        7bddff8c01f5fb931c3cb73d4aa8e66d303d97bc
)

FetchContent_MakeAvailable(dx8)

# The libraries of the min-dx8-sdk are x86 only. For x64, d3d8.dll is loaded at runtime anyway,
# DirectInput comes from the Windows SDK and D3DX8 is replaced by a stub, because it has no x64
# version. There is no x64 d3d8.dll either, so the W3D renderer only works in x86 builds.
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_LIBRARIES dinput8 dxguid legacy_stdio_definitions deps_d3dx8stub)
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_DIRECTORIES "")
    set_property(TARGET d3d8lib PROPERTY INTERFACE_LINK_OPTIONS /NODEFAULTLIB:libci.lib)
    set(RTS_USE_D3DX8_STUB ON)
endif()
