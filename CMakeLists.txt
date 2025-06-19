cmake_minimum_required(VERSION 3.5)

project(Demo01 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform specific settings
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE OFF)
elseif(UNIX AND NOT APPLE)
    # Linux specific settings
    add_definitions(-DQ_OS_LINUX)
    # Add threading support
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        mymessagebox.h
        mymessagebox.cpp
        worker.h
        worker.cpp
        savedsettingdialog.cpp
)

qt_add_executable(Demo01
    WIN32
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(Demo01 PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
)

if(UNIX AND NOT APPLE)
    target_link_libraries(Demo01 PRIVATE Threads::Threads)
endif()

set_target_properties(Demo01 PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE ON
)

qt_finalize_executable(Demo01)

include(GNUInstallDirs)

install(TARGETS Demo01
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Demo01
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
