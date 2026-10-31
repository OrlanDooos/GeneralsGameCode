# Stands in for d3dx8.lib in x64 builds, because D3DX8 only exists for x86. See cmake/dx8.cmake.
add_library(deps_d3dx8stub STATIC)

target_sources(deps_d3dx8stub PRIVATE
    d3dx8stub.cpp
)

# Only the DX8 headers, not d3d8lib itself, which links this library.
target_include_directories(deps_d3dx8stub PRIVATE
    ${dx8_SOURCE_DIR}
)
