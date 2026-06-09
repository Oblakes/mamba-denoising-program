cmake_minimum_required(VERSION 3.16)

project(QtNoiseReducer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    main.cpp
    MainWindow.h
    MainWindow.cpp
    WavFile.h
    WavFile.cpp
    AudioProcessor.h
    AudioProcessor.cpp
    SpectrumWidget.h
    SpectrumWidget.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QtNoiseReducer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(QtNoiseReducer ${PROJECT_SOURCES})
endif()

target_link_libraries(QtNoiseReducer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    target_link_libraries(QtNoiseReducer PRIVATE winmm)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_finalize_executable(QtNoiseReducer)
endif()
