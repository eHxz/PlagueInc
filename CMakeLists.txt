cmake_minimum_required(VERSION 3.16)

project(PlagueInc VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Xml Multimedia)


set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(PlagueInc
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        GameTypes.h
        GameCore.h
        gamecore.h gamecore.cpp
        mapwidget.h mapwidget.cpp
        populationbar.h populationbar.cpp
        skilltree.h skilltree.cpp
        hexnode.h hexnode.cpp
        soundmanager.h soundmanager.cpp
        hudpanel.h hudpanel.cpp
        diseasemenu.h diseasemenu.cpp
        worldmenu.h worldmenu.cpp
        notice.h notice.cpp
        regions.h
        gameparams.h
        transportdata.h
        res.qrc

    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET PlagueInc APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(PlagueInc SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(PlagueInc
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(PlagueInc PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Multimedia)

# 把 soundtrack 复制到可执行文件目录，运行时按 applicationDirPath()/soundtrack 读取音频。
add_custom_command(TARGET PlagueInc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/soundtrack"
            "$<TARGET_FILE_DIR:PlagueInc>/soundtrack"
    COMMENT "Copying soundtrack next to executable")

# 把背景视频复制到可执行文件目录，运行时按 applicationDirPath()/media/pathogen.mp4 读取。
# 用 ASCII 文件名（原始为“旋转病原体.mp4”，中文路径会让 Ninja/RCC 在中文环境下出错）。
add_custom_command(TARGET PlagueInc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:PlagueInc>/media"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/media/pathogen.mp4"
            "$<TARGET_FILE_DIR:PlagueInc>/media/pathogen.mp4"
    COMMENT "Copying background video next to executable")

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.PlagueInc)
endif()
set_target_properties(PlagueInc PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS PlagueInc
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(PlagueInc)
endif()
