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

if(APPLE)
    set_target_properties(mark-reader PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Mark Reader"
        MACOSX_BUNDLE_BUNDLE_NAME "Mark Reader"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.markreader.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

enable_testing()
add_executable(reader_tests tests/test_librarystore.cpp)
target_link_libraries(reader_tests PRIVATE reader_core Qt6::Test)
add_test(NAME reader_tests COMMAND reader_tests)

if(APPLE)
    add_test(NAME macos_app_bundle
        COMMAND ${CMAKE_COMMAND}
            -DBUNDLE=$<TARGET_BUNDLE_DIR:mark-reader>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_bundle.cmake
    )
endif()
