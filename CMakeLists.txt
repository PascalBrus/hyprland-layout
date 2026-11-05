cmake_minimum_required(VERSION 3.27)

project(threecol
    VERSION 0.1.0
    DESCRIPTION "A three-zone tiling layout for Hyprland"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(THREECOL_TESTS "Build the GoogleTest suite" ON)

# Core module: all layout behaviour, plain C++, no Hyprland headers.
add_library(threecol_core STATIC
    src/core/DividerStore.cpp
    src/core/RowHeights.cpp
    src/core/Settings.cpp
    src/core/Workspace.cpp)
target_include_directories(threecol_core PUBLIC src)
target_compile_options(threecol_core PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(threecol_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Plugin module: translates between Hyprland and the core module.
find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED IMPORTED_TARGET hyprland pixman-1 libdrm)

add_library(threecol SHARED
    src/plugin/main.cpp
    src/plugin/Config.cpp
    src/plugin/DropIndicator.cpp
    src/plugin/SavedDividers.cpp
    src/plugin/SettingsReporting.cpp
    src/plugin/ThreecolAlgorithm.cpp
    src/plugin/VersionGuard.cpp)
target_link_libraries(threecol PRIVATE threecol_core PkgConfig::HYPRLAND)
# Hyprland loads the plugin as threecol.so.
set_target_properties(threecol PROPERTIES PREFIX "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(threecol PRIVATE --no-gnu-unique)
endif()

if(THREECOL_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)

    add_executable(threecol_tests
        tests/core/FillOrderTest.cpp
        tests/core/CloseRulesTest.cpp
        tests/core/SettingsTest.cpp
        tests/core/KeyboardResizeTest.cpp
        tests/core/DividerStoreTest.cpp
        tests/core/MouseResizeTest.cpp
        tests/core/RowHeightsTest.cpp
        tests/core/RowLimitTest.cpp
        tests/core/ChainTest.cpp
        tests/core/DragDropTest.cpp
        tests/core/DropIndicatorTest.cpp
        tests/core/CarryAcrossRebuildTest.cpp
        tests/core/CentredMasterTest.cpp)
    target_link_libraries(threecol_tests PRIVATE threecol_core GTest::gtest_main)
    gtest_discover_tests(threecol_tests)

    add_test(NAME core_includes_no_hyprland_headers
        COMMAND ${CMAKE_COMMAND} -DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/core
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCoreIsPure.cmake)
endif()
