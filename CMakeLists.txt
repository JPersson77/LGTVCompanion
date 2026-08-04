cmake_minimum_required(VERSION 3.20)
project(lgtv-companion
    VERSION 5.6.0
    DESCRIPTION "Control LG webOS TVs used as PC displays"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_GUI "Build the Qt user interface" ON)

# ----------------------------------------------------------- dependencies ---

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json 3.9 REQUIRED)

# ------------------------------------------------------------- lgtv-common ---
# Portable core: device model, webOS JSON api, config, logging, local IPC.

add_library(lgtv-common STATIC
    src/common/event.cpp
    src/common/ipc.cpp
    src/common/log.cpp
    src/common/paths.cpp
    src/common/preferences.cpp
    src/common/tools.cpp
)

target_include_directories(lgtv-common PUBLIC src/common)

target_link_libraries(lgtv-common PUBLIC
    Boost::headers
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Boost.Asio pulls in Boost.System; keep it header-only and drop the deprecated
# implicit io_service aliases so the async code compiles clean.
target_compile_definitions(lgtv-common PUBLIC
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_DISABLE_BOOST_ARRAY
)

target_compile_options(lgtv-common PRIVATE -Wall -Wextra -Wno-unused-parameter)

# --------------------------------------------------------------- smoketest ---

enable_testing()

add_executable(lgtv-smoketest src/common/smoke_test.cpp)
target_link_libraries(lgtv-smoketest PRIVATE lgtv-common)
add_test(NAME smoketest COMMAND lgtv-smoketest)
