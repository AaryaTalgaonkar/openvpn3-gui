cmake_minimum_required(VERSION 3.16)

project(IITDelhiVPN VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/forms
)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

set(PROJECT_SOURCES




)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(IITDelhiVPN
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}






        src/main.cpp
        src/certificatedownloadservice.cpp


        src/mainwindow.cpp
        src/connectionprogresswidget.cpp
        src/trafficgraphwidget.cpp
        src/certificateboxwidget.cpp
        src/validitytimelinewidget.cpp
        src/userinfowidget.cpp
        src/statuswidget.cpp
        src/generateprogresswidget.cpp
        headers/winmacvpnbackend.h
        headers/mainwindow.h
        headers/connectionprogresswidget.h
        headers/trafficgraphwidget.h
        headers/certificateboxwidget.h
        headers/validitytimelinewidget.h
        headers/userinfowidget.h
        headers/statuswidget.h
        headers/generateprogresswidget.h
        forms/certificatebox.ui
        forms/validitytimeline.ui
        forms/userinfo.ui
        forms/statuswidget.ui
        forms/generateprogress.ui
        headers/certificatedownloadservice.h
        resources/resources.qrc
        forms/mainwindow.ui
        forms/connectscreen.ui
        forms/connectingscreen.ui
        forms/disconnectscreen.ui
        forms/getstarted.ui
        headers/linuxvpnbackend.h
        src/linuxvpnbackend.cpp
        headers/ivpnbackend.h
        src/winmacvpnbackend.cpp
        headers/ikeystore.h
        headers/windowskeystore.h
        src/windowskeystore.cpp
        headers/linuxkeystore.h
        src/linuxkeystore.cpp
        headers/mackeystore.h
        src/mackeystore.cpp


    )

set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/deploy/iitdelhivpn.icns)
set_source_files_properties(${APP_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)
target_sources(IITDelhiVPN PRIVATE ${APP_ICON})

    
target_include_directories(IITDelhiVPN
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/headers
)



# Define target properties for Android with Qt 6 as:
#    set_property(TARGET IITDelhiVPN APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(IITDelhiVPN SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(IITDelhiVPN
            ${PROJECT_SOURCES}
        )
    endif()
endif()





target_link_libraries(IITDelhiVPN PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Concurrent)

if(WIN32)
    target_link_libraries(IITDelhiVPN PRIVATE Crypt32)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.app)
endif()
set_target_properties(IITDelhiVPN PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE_ICON_FILE iitdelhivpn.icns
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS IITDelhiVPN
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IITDelhiVPN)
endif()
