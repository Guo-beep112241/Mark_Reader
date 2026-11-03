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

set(MARKREADER_UI_SOURCES
    src/mainwindow.cpp
    src/readerwidget.cpp
    src/retrowidgets.cpp
    src/settingsdialog.cpp
    src/annotationpanel.cpp
)

add_executable(mark-reader src/main.cpp ${MARKREADER_UI_SOURCES})
target_link_libraries(mark-reader PRIVATE reader_core Qt6::Widgets)
set(MARKREADER_RESOURCES
    resources/arrow-down.png
    resources/arrow-down@2x.png
    resources/app-icon.png
)
qt_add_resources(mark-reader "markreader_resources"
    PREFIX "/theme"
    BASE resources
    FILES ${MARKREADER_RESOURCES}
)

if(APPLE)
    set_target_properties(mark-reader PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Mark Reader"
        MACOSX_BUNDLE_BUNDLE_NAME "Mark Reader"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.markreader.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    )

    # 从 1024×1024 源图生成完整 iconset，再交给 iconutil 打包成 AppIcon.icns。
    set(APP_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.png")
    set(APP_ICON_SET "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.iconset")
    set(APP_ICON_ICNS "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns")
    add_custom_command(
        OUTPUT "${APP_ICON_ICNS}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${APP_ICON_SET}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_ICON_SET}"
        COMMAND sips -z 16 16 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_16x16.png"
        COMMAND sips -z 32 32 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_16x16@2x.png"
        COMMAND sips -z 32 32 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_32x32.png"
        COMMAND sips -z 64 64 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_32x32@2x.png"
        COMMAND sips -z 128 128 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_128x128.png"
        COMMAND sips -z 256 256 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_128x128@2x.png"
        COMMAND sips -z 256 256 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_256x256.png"
        COMMAND sips -z 512 512 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_256x256@2x.png"
        COMMAND sips -z 512 512 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_512x512.png"
        COMMAND sips -z 1024 1024 "${APP_ICON_SOURCE}" --out "${APP_ICON_SET}/icon_512x512@2x.png"
        COMMAND iconutil -c icns "${APP_ICON_SET}" -o "${APP_ICON_ICNS}"
        DEPENDS "${APP_ICON_SOURCE}"
        COMMENT "Generating AppIcon.icns from resources/app-icon.png"
        VERBATIM
    )
    set_source_files_properties("${APP_ICON_ICNS}" PROPERTIES
        GENERATED TRUE
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(mark-reader PRIVATE "${APP_ICON_ICNS}")
endif()

enable_testing()
add_executable(reader_tests tests/test_librarystore.cpp ${MARKREADER_UI_SOURCES})
target_link_libraries(reader_tests PRIVATE reader_core Qt6::Widgets Qt6::Test)
qt_add_resources(reader_tests "markreader_test_resources"
    PREFIX "/theme"
    BASE resources
    FILES ${MARKREADER_RESOURCES}
)
add_test(NAME reader_tests COMMAND reader_tests)
set_tests_properties(reader_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(APPLE)
    add_test(NAME macos_app_bundle
        COMMAND ${CMAKE_COMMAND}
            -DBUNDLE=$<TARGET_BUNDLE_DIR:mark-reader>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_bundle.cmake
    )
endif()
