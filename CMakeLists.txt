cmake_minimum_required(VERSION 3.24)
project(waku_screensaver LANGUAGES CXX)

add_library(screensaver-animation STATIC
  src/animation/animation.cpp
  src/animation/rain.cpp
  src/animation/decrypt.cpp
  src/animation/waves.cpp
  src/animation/beams.cpp
  src/animation/matrix.cpp
  src/animation/burn.cpp
  src/animation/blocks.cpp)
target_include_directories(screensaver-animation PUBLIC src)
target_compile_features(screensaver-animation PUBLIC cxx_std_23)
target_compile_options(screensaver-animation PRIVATE /W4 /WX /utf-8)

add_executable(waku-screensaver WIN32 src/win32/main.cpp src/win32/renderer.cpp)
target_compile_definitions(waku-screensaver PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(waku-screensaver PRIVATE /W4 /WX /utf-8)
target_link_libraries(waku-screensaver PRIVATE screensaver-animation d2d1 dwrite shell32)
set_target_properties(waku-screensaver PROPERTIES SUFFIX ".scr")
add_custom_command(TARGET waku-screensaver POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:waku-screensaver>" "$<TARGET_FILE_DIR:waku-screensaver>/waku-screensaver.exe")

enable_testing()
add_test(NAME self-test COMMAND waku-screensaver --self-test)
add_executable(animation-tests tests/animation_tests.cpp)
target_link_libraries(animation-tests PRIVATE screensaver-animation)
target_compile_options(animation-tests PRIVATE /W4 /WX /utf-8)
add_test(NAME animation-tests COMMAND animation-tests)

set_property(TARGET screensaver-animation waku-screensaver animation-tests PROPERTY MSVC_RUNTIME_LIBRARY
  "MultiThreaded$<$<CONFIG:Debug>:Debug>")
