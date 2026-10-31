cmake_minimum_required(VERSION 3.16)
project(HexaFronteraCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# El núcleo vive dentro del módulo de Unreal; acá se compila solo, sin el motor.
set(HF_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Unreal/HexaFrontera/Source/HexaFrontera/Core)
set(HF_SCENARIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Unreal/HexaFrontera/Content/Scenarios)

add_library(hfcore STATIC
  ${HF_CORE_DIR}/src/Json.cpp
  ${HF_CORE_DIR}/src/Hex.cpp
  ${HF_CORE_DIR}/src/Scenario.cpp
  ${HF_CORE_DIR}/src/Board.cpp
  ${HF_CORE_DIR}/src/Game.cpp
  ${HF_CORE_DIR}/src/Bot.cpp)
target_include_directories(hfcore PUBLIC ${HF_CORE_DIR}/include)
if(MSVC)
  target_compile_options(hfcore PRIVATE /W4 /WX)
else()
  target_compile_options(hfcore PRIVATE -Wall -Wextra -Wshadow -Wconversion -Werror)
endif()

add_executable(hfcore_tests tests.cpp)
target_link_libraries(hfcore_tests PRIVATE hfcore)
target_compile_definitions(hfcore_tests PRIVATE HF_SCENARIO_DIR="${HF_SCENARIO_DIR}")

add_executable(hf_simulate simulate.cpp)
target_link_libraries(hf_simulate PRIVATE hfcore)
target_compile_definitions(hf_simulate PRIVATE HF_SCENARIO_DIR="${HF_SCENARIO_DIR}")

enable_testing()
add_test(NAME hfcore_tests COMMAND hfcore_tests)
