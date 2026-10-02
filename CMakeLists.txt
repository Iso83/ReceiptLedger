cmake_minimum_required(VERSION 4.0)

project(MyProject
    VERSION 0.1.0
    LANGUAGES C CXX
)

# =========================================================
# Project configuration
# =========================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# =========================================================
# CppCMake
# =========================================================

include(cmake/Dependencies.cmake)

dependencies_fetch_cppcmake()

cppcmake_project_initialize()

if(CPPCMAKE_PROJECT_ALREADY_REGISTERED)
    return()
endif()

cppcmake_githook_precommit()
cppcmake_version_generate()
cppcmake_test_setup()

# =========================================================
# Dependencies
# =========================================================

#
# Load optional machine-local package configuration.
#
# CPPCMAKE_DEPENDENCY_LOCAL_PACKAGE_FILE may point to a local
# CMake file that configures prebuilt packages, package search
# paths, or machine-specific dependency helpers.
#
# When unset or unavailable, dependency resolution continues
# normally.
#

cppcmake_dependency_try_local_packages()

#
# Initialize dependency submodules that are not already
# provided by a parent project.
#
# PATH entries without TARGET are always initialized.
# PATH entries with TARGET are skipped when that CMake target
# already exists.
#

cppcmake_gitsubmodule_init_missing_targets(
    WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"

    PATH
        "extern/CppDependencies"

    PATH
        "extern/MyLibrary"
    TARGET
        "MyLibrary_core"
)

#
# Add dependency subdirectories only when their target is not
# already available from a parent project.
#

cppcmake_dependency_add_subdirectory_unless_target(
    MyLibrary_core
    extern/MyLibrary
)

#
# Add project dependencies here.
#
# Examples:
#
#   - find_package(...)
#   - FetchContent + cppcmake_dependency_make_available(...)
#   - cppcmake_dependency_add_subdirectory(...)
#   - cppcmake_dependency_add_subdirectory_unless_target(...)
#
# Optionally group dependency targets in the IDE using:
#
#   cppcmake_dependency_set_folder(...)
#

# =========================================================
# Modules
# =========================================================

#
# Register project modules here.
#
# Examples:
#
#   cppcmake_module_add(engine/core)
#   cppcmake_module_add(engine/math)
#
#   cppcmake_module_add(apps/demo)
#   cppcmake_module_add(apps/editor)
#
