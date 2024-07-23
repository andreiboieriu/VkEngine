#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "fastgltf::fastgltf_simdjson" for configuration ""
set_property(TARGET fastgltf::fastgltf_simdjson APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(fastgltf::fastgltf_simdjson PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libfastgltf_simdjson.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS fastgltf::fastgltf_simdjson )
list(APPEND _IMPORT_CHECK_FILES_FOR_fastgltf::fastgltf_simdjson "${_IMPORT_PREFIX}/lib/libfastgltf_simdjson.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
