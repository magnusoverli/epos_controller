cmake_minimum_required(VERSION 3.24)
project(HeadsetController VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The sysfs backend currently supports Linux only")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/ProjectOptions.cmake)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
configure_file(app/BuildInfo.h.in generated/BuildInfo.h @ONLY)

add_library(hid-descriptor STATIC app/HidDescriptor.cpp app/HidDescriptor.h)
target_include_directories(hid-descriptor PUBLIC app)
headset_target_options(hid-descriptor)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hid-descriptor PRIVATE -Wconversion -Wsign-conversion -Wshadow)
endif()

add_library(headset-core STATIC
    app/DeviceDiscovery.cpp app/DeviceDiscovery.h
    app/SysfsDeviceScanner.cpp app/SysfsDeviceScanner.h
    app/HidInspection.cpp app/HidInspection.h
    app/HidDescriptorPresentation.cpp app/HidDescriptorPresentation.h
    app/Diagnostics.cpp app/Diagnostics.h
)
headset_target_options(headset-core)
target_include_directories(headset-core PUBLIC app "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(headset-core PUBLIC Qt6::Core hid-descriptor)
qt_extract_metatypes(headset-core)

qt_add_executable(headset-controller
    app/main.cpp app/DiagnosticsController.cpp app/DiagnosticsController.h
)
set_source_files_properties(qml/Theme.qml qml/SettingsCatalog.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(headset-controller
    URI HeadsetController
    VERSION 1.0
    SOURCES app/QmlTypes.h
    QML_FILES
        qml/Main.qml qml/Theme.qml qml/Panel.qml qml/Heading.qml
        qml/OutlineButton.qml qml/HeadsetArt.qml qml/HomePage.qml qml/UpdatesPage.qml
        qml/SettingsCatalog.qml qml/SettingsPage.qml qml/HelpPage.qml
        qml/DiagnosticsDialog.qml
)
target_link_libraries(headset-controller PRIVATE headset-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
headset_target_options(headset-controller)

include(CTest)
if(BUILD_TESTING)
    foreach(test IN ITEMS discovery hid)
        qt_add_executable(${test}-test tests/${test}_test.cpp)
        target_link_libraries(${test}-test PRIVATE headset-core)
        headset_target_options(${test}-test)
        add_test(NAME ${test} COMMAND ${test}-test)
    endforeach()
    add_executable(hid-descriptor-test tests/hid_descriptor_test.cpp)
    set_target_properties(hid-descriptor-test PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(hid-descriptor-test PRIVATE hid-descriptor)
    headset_target_options(hid-descriptor-test)
    add_test(NAME hid-descriptor COMMAND hid-descriptor-test)
    foreach(page RANGE 0 3)
        add_test(NAME ui-page-${page} COMMAND headset-controller --smoke-test --page ${page})
        set_tests_properties(ui-page-${page} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 15)
    endforeach()
    add_test(NAME ui-diagnostics COMMAND headset-controller --smoke-test --open-diagnostics)
    set_tests_properties(ui-diagnostics PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 15)
    add_test(NAME cli-diagnostics COMMAND headset-controller --diagnostics)
    set_tests_properties(cli-diagnostics PROPERTIES ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY=;QT_QPA_PLATFORM=invalid")
    add_test(NAME cli-invalid-page COMMAND headset-controller --diagnostics --page invalid)
    set_tests_properties(cli-invalid-page PROPERTIES WILL_FAIL TRUE)
endif()
install(TARGETS headset-controller RUNTIME DESTINATION bin)
