cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

project(ImagePlus VERSION 1.0.4)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if(PROJECT_IS_TOP_LEVEL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMAGE_PLUS_EXPORTING)
endif()

if(DEFINED ENV{GITHUB_ACTIONS})
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    if (WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-incremental:no")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-incremental:no")
    endif()
else()
    message(STATUS "Including tests")
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMAGEPLUS_INCLUDE_TESTS)
endif()

# For file formats
# WebP
CPMAddPackage(
    NAME libwebp
    GITHUB_REPOSITORY "webmproject/libwebp"
    GIT_TAG "4510224"
    OPTIONS
        "WEBP_BUILD_CWEBP OFF"
        "WEBP_BUILD_DWEBP OFF"
        "WEBP_BUILD_IMG2WEBP OFF"
        "WEBP_BUILD_WEBPINFO OFF"
        "WEBP_BUILD_WEBPMUX OFF"
        "WEBP_BUILD_GIF2WEBP OFF"
        "WEBP_BUILD_ANIM_UTILS OFF"
        "WEBP_BUILD_EXTRAS OFF"
)
if(APPLE)
    # Some weird macOS SDK issue
    target_compile_definitions(webp PRIVATE _Float16=float)
endif()
target_link_libraries(${PROJECT_NAME} webp webpdemux webpencode libwebpmux)

# QOI
CPMAddPackage("gh:phoboslab/qoi#316593b")
target_include_directories(${PROJECT_NAME} PUBLIC ${qoi_SOURCE_DIR})

# spng
CPMAddPackage("gh:richgel999/miniz#c883286")
CPMAddPackage(
    NAME spng
    GITHUB_REPOSITORY "randy408/libspng"
    GIT_TAG "adc9439"
    DOWNLOAD_ONLY ON
)
add_library(spng STATIC ${spng_SOURCE_DIR}/spng/spng.c)
target_include_directories(spng PUBLIC ${spng_SOURCE_DIR}/spng)
target_compile_definitions(spng PUBLIC SPNG_STATIC SPNG_USE_MINIZ)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(miniz PRIVATE -Wno-unused-function -Wno-newline-eof)
endif()
target_link_libraries(spng PUBLIC miniz)
target_link_libraries(${PROJECT_NAME} spng)

# JXL
CPMAddPackage(
    NAME libjxl
    GITHUB_REPOSITORY "libjxl/libjxl"
    GIT_TAG "53042ec"
    OPTIONS
        "JPEGXL_ENABLE_TOOLS OFF"
        "JPEGXL_ENABLE_DOXYGEN OFF"
        "JPEGXL_ENABLE_FUZZERS OFF"
        "JPEGXL_ENABLE_MANPAGES OFF"
        "JPEGXL_ENABLE_BENCHMARK OFF"
        "JPEGXL_ENABLE_COVERAGE OFF"
        "JPEGXL_ENABLE_EXAMPLES OFF"
        "JPEGXL_ENABLE_OPENEXR OFF"
        "JPEGXL_ENABLE_JPEGLI OFF"
        "JPEGXL_ENABLE_JPEGLI_LIBJPEG OFF"
        "JPEGXL_BUNDLE_LIBPNG OFF"
        "JPEGXL_ENABLE_SJPEG OFF"
        "JPEGXL_ENABLE_JNI OFF"
        "BROTLI_BUNDLED_MODE ON"
        "BUILD_TESTING OFF"
        "BUILD_SHARED_LIBS OFF"
)
target_compile_definitions(jxl PUBLIC JXL_STATIC_DEFINE)
target_link_libraries(${PROJECT_NAME} jxl jxl_threads)

# stb
CPMAddPackage("gh:nothings/stb#fede005")
target_include_directories(${PROJECT_NAME} PUBLIC ${stb_SOURCE_DIR})

setup_geode_mod(${PROJECT_NAME})
