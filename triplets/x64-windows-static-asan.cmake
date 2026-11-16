set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# Release only: vcpkg's debug builds add /RTC1, which /fsanitize=address rejects.
set(VCPKG_BUILD_TYPE release)

# Dependencies must match the project's ASan setting, or MSVC's STL annotation checks
# (annotate_string, annotate_vector, annotate_optional) fail at link time.
set(VCPKG_C_FLAGS "/fsanitize=address")
set(VCPKG_CXX_FLAGS "/fsanitize=address")
