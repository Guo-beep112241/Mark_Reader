if(NOT EXISTS "${BUNDLE}/Contents/Info.plist")
    message(FATAL_ERROR "macOS app bundle is missing Contents/Info.plist: ${BUNDLE}")
endif()

if(NOT EXISTS "${BUNDLE}/Contents/MacOS/Mark Reader")
    message(FATAL_ERROR "macOS app bundle is missing its executable: ${BUNDLE}")
endif()
