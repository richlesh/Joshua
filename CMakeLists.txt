cmake_minimum_required(VERSION 3.21)
project(Joshua VERSION 1.8.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# AGL.framework removed in Xcode 26 — pre-create WrapOpenGL target without it
if(APPLE)
  find_package(OpenGL)
  if(OpenGL_FOUND AND NOT TARGET WrapOpenGL::WrapOpenGL)
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
    target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "-framework OpenGL")
  endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(SOURCES
  src/main.cpp
  src/MainWindow.cpp
  src/TicTacToeWindow.cpp
  src/CheckersWindow.cpp
  src/CheckersEndgame.cpp
  src/chinook_db.cpp
  src/FourAcrossWindow.cpp
  src/ChessWindow.cpp
  src/GoWindow.cpp
  src/ReversiWindow.cpp
  src/BlackjackWindow.cpp
  src/PokerWindow.cpp
  src/ThermonuclearWarWindow.cpp
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
  src/CheckersWindow.h
  src/CheckersEndgame.h
  src/chinook_db.h
  src/FourAcrossWindow.h
  src/ChessWindow.h
  src/GoWindow.h
  src/ReversiWindow.h
  src/BlackjackWindow.h
  src/PokerWindow.h
  src/ThermonuclearWarWindow.h
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

# Chinook DB code - suppress warnings from legacy code
if(MSVC)
  set_source_files_properties(src/chinook_db.cpp PROPERTIES COMPILE_FLAGS "/w")
else()
  set_source_files_properties(src/chinook_db.cpp PROPERTIES COMPILE_FLAGS "-Wno-everything")
endif()
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

# Bundle Chinook endgame database
set(DB6_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/resources/DB6.zip")
if(EXISTS ${DB6_ZIP})
  if(APPLE)
    set_source_files_properties(${DB6_ZIP}
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(Joshua PRIVATE ${DB6_ZIP})
  elseif(WIN32)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DB6_ZIP} $<TARGET_FILE_DIR:Joshua>/DB6.zip)
  endif()
endif()

if(WIN32)
  target_sources(Joshua PRIVATE resources/app_icon.rc)
endif()

# Bundle Stockfish chess engine
set(STOCKFISH_BIN "${CMAKE_CURRENT_SOURCE_DIR}/resources/stockfish${CMAKE_EXECUTABLE_SUFFIX}")
if(EXISTS ${STOCKFISH_BIN})
  if(APPLE)
    set_source_files_properties(${STOCKFISH_BIN}
      PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(Joshua PRIVATE ${STOCKFISH_BIN})
    # Ad-hoc sign the bundled binary so macOS allows execution
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:Joshua>/Contents/Resources/stockfish")
  elseif(WIN32)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STOCKFISH_BIN} $<TARGET_FILE_DIR:Joshua>/stockfish.exe)
  endif()
endif()

# Bundle KataGo Go engine + model + config
set(KATAGO_BIN "${CMAKE_CURRENT_SOURCE_DIR}/resources/katago${CMAKE_EXECUTABLE_SUFFIX}")
set(KATAGO_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/resources/katago-model.bin.gz")
set(KATAGO_CFG "${CMAKE_CURRENT_SOURCE_DIR}/resources/katago-gtp.cfg")
if(EXISTS ${KATAGO_BIN})
  if(APPLE)
    set_source_files_properties(${KATAGO_BIN} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(Joshua PRIVATE ${KATAGO_BIN})
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:Joshua>/Contents/Resources/katago")
    if(EXISTS ${KATAGO_MODEL})
      set_source_files_properties(${KATAGO_MODEL} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
      target_sources(Joshua PRIVATE ${KATAGO_MODEL})
    endif()
    if(EXISTS ${KATAGO_CFG})
      set_source_files_properties(${KATAGO_CFG} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
      target_sources(Joshua PRIVATE ${KATAGO_CFG})
    endif()
  elseif(WIN32)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_BIN} $<TARGET_FILE_DIR:Joshua>/katago.exe)
    if(EXISTS ${KATAGO_MODEL})
      add_custom_command(TARGET Joshua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_MODEL} $<TARGET_FILE_DIR:Joshua>/katago-model.bin.gz)
    endif()
    if(EXISTS ${KATAGO_CFG})
      add_custom_command(TARGET Joshua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_CFG} $<TARGET_FILE_DIR:Joshua>/katago-gtp.cfg)
    endif()
  endif()
endif()

if(UNIX AND NOT APPLE)
  install(TARGETS Joshua RUNTIME DESTINATION bin)
  install(FILES resources/icons/app_icon_256.png DESTINATION share/icons/hicolor/256x256/apps RENAME joshua.png)
  install(FILES resources/Joshua.desktop DESTINATION share/applications)
  install(FILES resources/joshua-mime.xml DESTINATION share/mime/packages)
  if(EXISTS ${DB6_ZIP})
    install(FILES ${DB6_ZIP} DESTINATION share/joshua)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DB6_ZIP} $<TARGET_FILE_DIR:Joshua>/DB6.zip)
  endif()
  if(EXISTS ${STOCKFISH_BIN})
    install(FILES ${STOCKFISH_BIN} DESTINATION share/joshua
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STOCKFISH_BIN} $<TARGET_FILE_DIR:Joshua>/stockfish)
  endif()
  if(EXISTS ${KATAGO_BIN})
    install(FILES ${KATAGO_BIN} DESTINATION share/joshua
      PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    add_custom_command(TARGET Joshua POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_BIN} $<TARGET_FILE_DIR:Joshua>/katago)
    if(EXISTS ${KATAGO_MODEL})
      install(FILES ${KATAGO_MODEL} DESTINATION share/joshua)
      add_custom_command(TARGET Joshua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_MODEL} $<TARGET_FILE_DIR:Joshua>/katago-model.bin.gz)
    endif()
    if(EXISTS ${KATAGO_CFG})
      install(FILES ${KATAGO_CFG} DESTINATION share/joshua)
      add_custom_command(TARGET Joshua POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${KATAGO_CFG} $<TARGET_FILE_DIR:Joshua>/katago-gtp.cfg)
    endif()
  endif()
endif()
