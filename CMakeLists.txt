cmake_minimum_required(VERSION 3.16)
project(DataFrame CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(MINGW)
  # Link the C++ runtime statically: otherwise a different libstdc++-6.dll on
  # PATH (Git for Windows ships one) gets picked up and exceptions crash.
  add_link_options(-static)
endif()

# The library itself: columnar DataFrame, views, group-by, CSV.
add_library(dataframe STATIC
  Column.cpp
  CsvReader.cpp
  DataFrame.cpp
  DataFrameView.cpp
  GroupedDataFrame.cpp
)
target_include_directories(dataframe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# CLI: benchmark (default), --run and --trace pipelines.
add_executable(DataFrame main.cpp)
target_link_libraries(DataFrame PRIVATE dataframe)

enable_testing()
add_executable(dataframe_tests tests/test_dataframe.cpp)
target_link_libraries(dataframe_tests PRIVATE dataframe)
add_test(NAME dataframe_tests COMMAND dataframe_tests ${CMAKE_CURRENT_SOURCE_DIR}/web/data)
