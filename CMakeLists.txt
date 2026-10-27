cmake_minimum_required(VERSION 3.20)

project(FontMaker VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(FONTMAKER_BUILD_WINDOWS "Build the Win32 GUI and command-line tools" ${WIN32})
option(FONTMAKER_BUILD_TESTS "Build the core unit tests" ON)
option(FONTMAKER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(fontmaker_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        if(FONTMAKER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(FONTMAKER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(fontmaker_core STATIC
    src/core/src/charset.cpp
    src/core/src/io.cpp
    src/core/src/metadata.cpp
    src/core/src/packing.cpp
    src/core/src/sha256.cpp
    src/core/src/utf.cpp
    src/core/src/writer.cpp
)
add_library(FontMaker::Core ALIAS fontmaker_core)
target_include_directories(fontmaker_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include
)
target_compile_features(fontmaker_core PUBLIC cxx_std_20)
fontmaker_enable_warnings(fontmaker_core)

configure_file(
    src/core/include/fontmaker/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/fontmaker/version.hpp
    @ONLY
)
target_include_directories(fontmaker_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

if(WIN32 AND FONTMAKER_BUILD_WINDOWS)
    enable_language(RC)
    if(MINGW)
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -c 65001")
    endif()

    configure_file(src/ui/resource.h ${CMAKE_CURRENT_BINARY_DIR}/resource.h COPYONLY)
    configure_file(assets/icons/fontmaker.ico ${CMAKE_CURRENT_BINARY_DIR}/fontmaker.ico COPYONLY)
    configure_file(assets/icons/logo.bmp ${CMAKE_CURRENT_BINARY_DIR}/logo.bmp COPYONLY)
    configure_file(src/ui/app.manifest ${CMAKE_CURRENT_BINARY_DIR}/app.manifest COPYONLY)
    set(FONTMAKER_RESOURCE_H "${CMAKE_CURRENT_BINARY_DIR}/resource.h")
    set(FONTMAKER_ICON_PATH "${CMAKE_CURRENT_BINARY_DIR}/fontmaker.ico")
    set(FONTMAKER_LOGO_PATH "${CMAKE_CURRENT_BINARY_DIR}/logo.bmp")
    set(FONTMAKER_MANIFEST_PATH "${CMAKE_CURRENT_BINARY_DIR}/app.manifest")
    configure_file(src/ui/resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.rc @ONLY)

    add_library(fontmaker_win32 STATIC
        src/platform/windows/font_catalog.cpp
        src/platform/windows/gdi_rasterizer.cpp
        src/platform/windows/win32_utils.cpp
    )
    add_library(FontMaker::Win32 ALIAS fontmaker_win32)
    target_include_directories(fontmaker_win32 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows
    )
    target_compile_definitions(fontmaker_win32 PUBLIC
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(fontmaker_win32 PUBLIC
        FontMaker::Core
        gdi32
        user32
        comdlg32
    )
    fontmaker_enable_warnings(fontmaker_win32)

    add_executable(fontmaker_cli src/cli/main.cpp)
    target_link_libraries(fontmaker_cli PRIVATE FontMaker::Win32 shell32)
    fontmaker_enable_warnings(fontmaker_cli)

    add_executable(fontmaker WIN32
        src/ui/about_dialog.cpp
        src/ui/app.cpp
        src/ui/charset_editor.cpp
        src/ui/main_window.cpp
        src/ui/ui_settings.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/resources.rc
    )
    target_include_directories(fontmaker PRIVATE
        src/ui
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(fontmaker PRIVATE FontMaker::Win32 comctl32 advapi32)
    set_target_properties(fontmaker PROPERTIES
        OUTPUT_NAME "FontMaker"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:fontmaker>"
    )
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT fontmaker)
    fontmaker_enable_warnings(fontmaker)
    if(MSVC)
        target_link_options(fontmaker PRIVATE /MANIFEST:NO)
    endif()

    add_custom_command(TARGET fontmaker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/charset
            $<TARGET_FILE_DIR:fontmaker>/charset
        COMMENT "Copying charset tables next to FontMaker.exe"
    )
    add_custom_command(TARGET fontmaker_cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/charset
            $<TARGET_FILE_DIR:fontmaker_cli>/charset
        COMMENT "Copying charset tables next to fontmaker_cli.exe"
    )

    install(TARGETS fontmaker fontmaker_cli RUNTIME DESTINATION .)
    install(DIRECTORY assets/charset/ DESTINATION charset)
    install(FILES README.md CHANGELOG.md DESTINATION .)
    install(DIRECTORY docs/ DESTINATION docs)
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_NAME "FontMaker")
    set(CPACK_PACKAGE_VENDOR "FontMaker contributors")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FONTMAKER_PACKAGE_ARCH "win64")
    else()
        set(FONTMAKER_PACKAGE_ARCH "win32")
    endif()
    set(CPACK_PACKAGE_FILE_NAME "FontMaker-${PROJECT_VERSION}-${FONTMAKER_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "FontMaker")
    include(CPack)
endif()

include(CTest)
if(FONTMAKER_BUILD_TESTS AND BUILD_TESTING)
    add_executable(fontmaker_tests
        tests/test_charset.cpp
        tests/test_main.cpp
        tests/test_metadata.cpp
        tests/test_packing.cpp
        tests/test_utf.cpp
        tests/test_writer.cpp
    )
    target_link_libraries(fontmaker_tests PRIVATE FontMaker::Core)
    fontmaker_enable_warnings(fontmaker_tests)
    add_test(NAME fontmaker_core_tests COMMAND fontmaker_tests)
    set_tests_properties(fontmaker_core_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    if(WIN32 AND FONTMAKER_BUILD_WINDOWS)
        add_test(NAME fontmaker_cli_ascii
            COMMAND ${CMAKE_COMMAND}
                -DCLI=$<TARGET_FILE:fontmaker_cli>
                -DCHARSET=${CMAKE_CURRENT_SOURCE_DIR}/assets/charset/ASCII.cst
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-ascii-test.bin
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli.cmake
        )
        set_tests_properties(fontmaker_cli_ascii PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )

        add_test(NAME fontmaker_cli_empty_guard
            COMMAND ${CMAKE_COMMAND}
                -DCLI=$<TARGET_FILE:fontmaker_cli>
                -DCHARSET=${CMAKE_CURRENT_SOURCE_DIR}/assets/charset/ASCII.cst
                -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-empty-test.bin
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli_empty.cmake
        )
        set_tests_properties(fontmaker_cli_empty_guard PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endif()
