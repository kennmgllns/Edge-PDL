
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "src")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/global_defs.h)
file(STRINGS src/global_defs.h GLOBAL_DEFS REGEX "^#define.*")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# set(PROJECT_VER "<major>.<minor>.<test>")
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" PROJECT_VER ${GLOBAL_DEFS})

# set(PROJECT_NAME <app-name>)
project(phase-data-logger)
