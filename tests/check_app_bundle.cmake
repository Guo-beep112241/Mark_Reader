if(NOT EXISTS "${BUNDLE}/Contents/Info.plist")
    message(FATAL_ERROR "macOS app bundle is missing Contents/Info.plist: ${BUNDLE}")
endif()

if(NOT EXISTS "${BUNDLE}/Contents/MacOS/Mark Reader")
    message(FATAL_ERROR "macOS app bundle is missing its executable: ${BUNDLE}")
endif()

set(APP_ICON "${BUNDLE}/Contents/Resources/AppIcon.icns")
if(NOT EXISTS "${APP_ICON}")
    message(FATAL_ERROR "macOS app bundle is missing Contents/Resources/AppIcon.icns: ${BUNDLE}")
endif()

file(READ "${APP_ICON}" ICON_MAGIC LIMIT 4 HEX)
if(NOT ICON_MAGIC STREQUAL "69636e73")
    message(FATAL_ERROR "AppIcon.icns is not a valid .icns file (magic: ${ICON_MAGIC})")
endif()

file(READ "${BUNDLE}/Contents/Info.plist" INFO_PLIST)
string(REGEX MATCH "<key>CFBundleIconFile</key>[ \t\r\n]*<string>([^<]*)</string>" ICON_KEY "${INFO_PLIST}")
if(NOT CMAKE_MATCH_1 STREQUAL "AppIcon.icns")
    message(FATAL_ERROR
        "Info.plist CFBundleIconFile must point to AppIcon.icns (found: '${CMAKE_MATCH_1}')")
endif()
