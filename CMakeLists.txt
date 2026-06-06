cmake_minimum_required(VERSION 3.21)
project(Joshua VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(SOURCES
  src/main.cpp
  src/MainWindow.cpp
  src/TicTacToeWindow.cpp
  src/BouncingProgressBar.cpp
  src/SettingsDialog.cpp
  src/LicenseDialog.cpp
  src/SplashScreen.cpp
  src/AboutDialog.cpp
  src/Settings.cpp
  src/LicenseValidator.cpp
  src/License.cpp
  src/Utilities.cpp
)

set(HEADERS
  src/MainWindow.h
  src/TicTacToeWindow.h
  src/BouncingProgressBar.h
  src/SettingsDialog.h
  src/LicenseDialog.h
  src/SplashScreen.h
  src/AboutDialog.h
  src/Settings.h
  src/LicenseValidator.h
  src/License.h
  src/Utilities.h
)

set(UI_FILES
  ui/MainWindow.ui
  ui/SettingsDialog.ui
  ui/LicenseDialog.ui
  ui/AboutDialog.ui
)

set(RESOURCES
  resources/resources.qrc
)

qt_add_executable(Joshua
  ${SOURCES}
  ${HEADERS}
  ${UI_FILES}
  ${RESOURCES}
)

target_compile_definitions(Joshua PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_include_directories(Joshua PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Joshua PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

set_target_properties(Joshua PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_ICON_FILE app_icon
  MACOSX_BUNDLE_BUNDLE_NAME "Joshua"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  WIN32_EXECUTABLE TRUE
)

set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.icns")
set_source_files_properties(${APP_ICON}
  PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
target_sources(Joshua PRIVATE ${APP_ICON})

if(WIN32)
  target_sources(Joshua PRIVATE resources/app_icon.rc)
endif()

if(UNIX AND NOT APPLE)
  install(TARGETS Joshua RUNTIME DESTINATION bin)
  install(FILES resources/icons/app_icon_256.png DESTINATION share/icons/hicolor/256x256/apps RENAME joshua.png)
  install(FILES resources/Joshua.desktop DESTINATION share/applications)
  install(FILES resources/joshua-mime.xml DESTINATION share/mime/packages)
endif()
