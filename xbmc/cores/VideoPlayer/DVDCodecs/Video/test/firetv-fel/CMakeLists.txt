cmake_minimum_required(VERSION 3.20)
project(AndroidFELTests LANGUAGES CXX)

# Standalone: runs on the host or Android without building the Kodi dependencies.
find_path(DVFELPLAY_INCLUDE_DIR dvfelplay.h REQUIRED)
add_executable(android-fel-tests ../../AndroidFEL.cpp TestAndroidFEL.cpp)
target_include_directories(android-fel-tests PRIVATE stubs ../.. ${DVFELPLAY_INCLUDE_DIR})
target_compile_features(android-fel-tests PRIVATE cxx_std_20)
target_compile_options(android-fel-tests PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME android-fel-lifecycle COMMAND android-fel-tests)
