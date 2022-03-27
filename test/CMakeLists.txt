cmake_minimum_required (VERSION 3.20)

find_package(GTest CONFIG REQUIRED)
file(GLOB_RECURSE PIPER_TEST_SRC "*.cpp")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(PiperTest ${PIPER_TEST_SRC} $<TARGET_OBJECTS:Piper>) #NOTICE: directly link objects for static factory
target_link_libraries(PiperTest PRIVATE Piper)
target_link_libraries(PiperTest PRIVATE GTest::gtest GTest::gtest_main)

add_test(NAME PiperTest COMMAND PiperTest)
