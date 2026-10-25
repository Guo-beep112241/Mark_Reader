cmake_minimum_required(VERSION 3.21)
project(MarkReader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

add_library(reader_core src/book.cpp src/librarystore.cpp)
target_include_directories(reader_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reader_core PUBLIC Qt6::Core)

add_executable(mark-reader src/main.cpp src/mainwindow.cpp src/readerwidget.cpp)
target_link_libraries(mark-reader PRIVATE reader_core Qt6::Widgets)

enable_testing()
add_executable(reader_tests tests/test_librarystore.cpp)
target_link_libraries(reader_tests PRIVATE reader_core Qt6::Test)
add_test(NAME reader_tests COMMAND reader_tests)
