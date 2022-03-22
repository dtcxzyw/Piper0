cmake_minimum_required (VERSION 3.20)

find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE PIPER_TEST_SRC "*.cpp")

add_executable(PiperTest ${PIPER_TEST_SRC})
target_link_libraries(PiperTest PRIVATE Piper)
target_link_libraries(PiperTest PRIVATE GTest::gtest GTest::gtest_main)

add_test(NAME PiperTest COMMAND PiperTest)
