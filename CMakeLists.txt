cmake_minimum_required(VERSION 3.21)
project(MarkReader VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(libzip CONFIG REQUIRED)

add_library(reader_core src/book.cpp src/epubdocument.cpp src/librarystore.cpp
    src/coverart.cpp src/theme.cpp)
target_include_directories(reader_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reader_core PUBLIC Qt6::Core Qt6::Gui libzip::zip)

add_executable(mark-reader src/main.cpp src/mainwindow.cpp src/readerwidget.cpp src/retrowidgets.cpp)
target_link_libraries(mark-reader PRIVATE reader_core Qt6::Widgets)
qt_add_resources(mark-reader "markreader_resources"
    PREFIX "/theme"
    BASE resources
    FILES resources/arrow-down.png resources/arrow-down@2x.png
)

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
add_executable(reader_tests tests/test_librarystore.cpp src/readerwidget.cpp src/retrowidgets.cpp)
target_link_libraries(reader_tests PRIVATE reader_core Qt6::Widgets Qt6::Test)
add_test(NAME reader_tests COMMAND reader_tests)
set_tests_properties(reader_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(APPLE)
    add_test(NAME macos_app_bundle
        COMMAND ${CMAKE_COMMAND}
            -DBUNDLE=$<TARGET_BUNDLE_DIR:mark-reader>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_bundle.cmake
    )
endif()
