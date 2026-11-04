cmake_minimum_required(VERSION 3.22)
project(NekoScope VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(NEKOSCOPE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(NEKOSCOPE_BUILD_GUI "Build the Qt Quick application" ON)
include(CTest)
include(GNUInstallDirs)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Concurrent)
set(CMAKE_AUTOMOC ON)

add_library(neko_options INTERFACE)
target_compile_options(neko_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
if(NEKOSCOPE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(neko_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(neko_options INTERFACE -fsanitize=address,undefined)
endif()

file(GLOB INVENTORY_SOURCES CONFIGURE_DEPENDS native/inventory/*.cpp)
file(GLOB SENSOR_SOURCES CONFIGURE_DEPENDS native/sensors/*.cpp)
add_library(neko_core STATIC
    native/InventoryCollector.cpp ${INVENTORY_SOURCES}
    native/core/SensorService.cpp ${SENSOR_SOURCES}
    native/core/TestRunner.cpp
    native/app/Preferences.cpp
    native/app/ReportBuilder.cpp
    native/app/HardwareService.cpp
    native/app/ReportService.cpp)
target_include_directories(neko_core PUBLIC native)
target_link_libraries(neko_core PUBLIC Qt6::Core Qt6::Concurrent neko_options)

if(NEKOSCOPE_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Qml Quick QuickControls2)
    qt_standard_project_setup()
    qt_add_executable(nekoscope native/app/main.cpp)
    target_link_libraries(nekoscope PRIVATE neko_core Qt6::Quick Qt6::QuickControls2)
    file(GLOB_RECURSE NEKO_QML CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} native/qml/*.qml)
    foreach(qml IN LISTS NEKO_QML)
        string(REPLACE "native/qml/" "" qml_alias "${qml}")
        set_source_files_properties(${qml} PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
    endforeach()
    qt_add_qml_module(nekoscope URI NekoScope VERSION 1.0 RESOURCE_PREFIX /qt/qml
        QML_FILES ${NEKO_QML})
    install(TARGETS nekoscope RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/io.nekoscope.NekoScope.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/io.nekoscope.NekoScope.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES public/favicon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME io.nekoscope.NekoScope.svg)
endif()

if(BUILD_TESTING)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    foreach(suite inventory sensors testrunner preferences reports)
        add_executable(tst_${suite} native/tests/tst_${suite}.cpp)
        target_link_libraries(tst_${suite} PRIVATE neko_core Qt6::Test)
        add_test(NAME ${suite} COMMAND tst_${suite})
        set_tests_properties(${suite} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    endforeach()
endif()
