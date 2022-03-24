cmake_minimum_required (VERSION 3.20)

find_package(GTest CONFIG REQUIRED)

find_package(TBB CONFIG REQUIRED)
find_package(OpenImageDenoise CONFIG REQUIRED)
find_package(embree 3.13.2 CONFIG REQUIRED)

file(GLOB_RECURSE PIPER_TEST_SRC "*.cpp")

add_executable(PiperTest ${PIPER_TEST_SRC} $<TARGET_OBJECTS:Piper>) #NOTICE: directly link objects for static factory
target_link_libraries(PiperTest PRIVATE Piper)
target_link_libraries(PiperTest PRIVATE GTest::gtest GTest::gtest_main)

# FIXME: reduce duplicate copy
add_custom_command(TARGET PiperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:OpenImageDenoise> $<TARGET_FILE_DIR:PiperTest>
)
add_custom_command(TARGET PiperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:embree> $<TARGET_FILE_DIR:PiperTest>
)
add_custom_command(TARGET PiperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:TBB::tbb> $<TARGET_FILE_DIR:PiperTest>
)
add_custom_command(TARGET PiperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:TBB::tbbmalloc> $<TARGET_FILE_DIR:PiperTest>
)
add_custom_command(TARGET PiperTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:TBB::tbbmalloc_proxy> $<TARGET_FILE_DIR:PiperTest>
)

add_test(NAME PiperTest COMMAND PiperTest)
