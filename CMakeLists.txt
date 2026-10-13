cmake_minimum_required(VERSION 3.24)
project(HeadsetController VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/ProjectOptions.cmake)
include(CheckCXXSourceCompiles)
option(HEADSET_PARSER_ONLY "Build parser/tests without Qt or Linux dependencies" OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
check_cxx_source_compiles("#include <expected>
int main() { std::expected<int, int> value(1); return *value; }" HEADSET_HAS_EXPECTED)
if(NOT HEADSET_HAS_EXPECTED)
    message(FATAL_ERROR "The compiler and standard library must support C++23 std::expected")
endif()
if(NOT HEADSET_PARSER_ONLY)
    check_cxx_source_compiles("#include <functional>
int main() { std::move_only_function<int()> f = [] { return 1; }; return f(); }" HEADSET_HAS_MOVE_ONLY_FUNCTION)
    if(NOT HEADSET_HAS_MOVE_ONLY_FUNCTION)
        message(FATAL_ERROR "The application requires C++23 std::move_only_function in the standard library")
    endif()
endif()
option(HEADSET_FUZZING "Build a Clang libFuzzer target" OFF)
option(HEADSET_BENCHMARKS "Build the repeatable discovery benchmark" OFF)
option(HEADSET_DEPLOY_QT "Deploy a matching Qt runtime beside the installed executable" ON)
include(CTest)

add_library(hid-descriptor STATIC app/HidDescriptor.cpp app/HidDescriptor.h)
set_target_properties(hid-descriptor PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(hid-descriptor PUBLIC app)
headset_target_options(hid-descriptor)
headset_strict_warnings(hid-descriptor)
if(BUILD_TESTING)
    add_executable(hid-descriptor-test tests/hid_descriptor_test.cpp)
    target_link_libraries(hid-descriptor-test PRIVATE hid-descriptor)
    headset_target_options(hid-descriptor-test)
    add_test(NAME hid-descriptor COMMAND hid-descriptor-test)
endif()
if(HEADSET_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "HEADSET_FUZZING requires Clang with libFuzzer")
    endif()
    add_executable(hid-descriptor-fuzz tests/hid_descriptor_fuzz.cpp app/HidDescriptor.cpp)
    target_include_directories(hid-descriptor-fuzz PRIVATE app)
    target_compile_features(hid-descriptor-fuzz PRIVATE cxx_std_23)
    target_compile_options(hid-descriptor-fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(hid-descriptor-fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
if(HEADSET_PARSER_ONLY)
    return()
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The device backend supports Linux; use HEADSET_PARSER_ONLY for portable parser builds")
endif()

# The supported baseline is now the SDK actually verified by this project.
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)
qt_standard_project_setup(REQUIRES 6.12)
configure_file(app/BuildInfo.h.in generated/BuildInfo.h @ONLY)

add_library(headset-core STATIC
    app/CommandLine.cpp app/CommandLine.h app/Pages.h
    app/DeviceDiscovery.cpp app/DeviceDiscovery.h
    app/SelectedDevice.cpp app/SelectedDevice.h
    app/DeviceSnapshot.cpp app/DeviceSnapshot.h
    app/SysfsReader.cpp app/SysfsReader.h
    app/SysfsDeviceScanner.cpp app/SysfsDeviceScanner.h
    app/HidInspection.cpp app/HidInspection.h
    app/HidDescriptorPresentation.cpp app/HidDescriptorPresentation.h
    app/Diagnostics.cpp app/Diagnostics.h
)
headset_target_options(headset-core)
headset_strict_warnings(headset-core)
target_include_directories(headset-core PUBLIC app "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(headset-core PUBLIC Qt6::Core hid-descriptor)
qt_extract_metatypes(headset-core)

set_source_files_properties(qml/Theme.qml qml/SettingsCatalog.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(headset-ui STATIC
    URI HeadsetController
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/HeadsetController"
    CLASS_NAME HeadsetControllerPlugin
    VERSION 1.0
    SOURCES app/QmlTypes.h app/DiagnosticsController.cpp app/DiagnosticsController.h
    QML_FILES
        qml/Main.qml qml/Theme.qml qml/Panel.qml qml/Heading.qml
        qml/OutlineButton.qml qml/HeadsetArt.qml qml/HomePage.qml qml/UpdatesPage.qml
        qml/SettingsCatalog.qml qml/SettingsPage.qml qml/HelpPage.qml qml/HelpCard.qml
        qml/DiagnosticsDialog.qml qml/LazyPage.qml
)
target_link_libraries(headset-ui PUBLIC headset-core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
headset_target_options(headset-ui)
qt_add_executable(headset-controller app/main.cpp app/DeviceMonitor.cpp app/DeviceMonitor.h)
target_link_libraries(headset-controller PRIVATE headset-ui headset-uiplugin PkgConfig::UDEV)
headset_target_options(headset-controller)
headset_strict_warnings(headset-controller)
add_custom_target(headset-controller_qmllint DEPENDS headset-ui_qmllint)

if(BUILD_TESTING)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    function(headset_ui_test name)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 20)
    endfunction()
    foreach(test IN ITEMS discovery hid)
        qt_add_executable(${test}-test tests/${test}_test.cpp)
        target_link_libraries(${test}-test PRIVATE headset-core Qt6::Test)
        headset_target_options(${test}-test)
        add_test(NAME ${test} COMMAND ${test}-test)
        set_tests_properties(${test} PROPERTIES TIMEOUT 20)
    endforeach()
    qt_add_executable(ui-test tests/ui_test.cpp)
    target_link_libraries(ui-test PRIVATE headset-ui headset-uiplugin Qt6::Test)
    headset_target_options(ui-test)
    add_test(NAME ui-interaction COMMAND ui-test)
    headset_ui_test(ui-interaction)
    add_test(NAME ui-high-dpi COMMAND ui-test)
    headset_ui_test(ui-high-dpi)
    set_property(TEST ui-high-dpi APPEND PROPERTY ENVIRONMENT "QT_SCALE_FACTOR=2")
    add_test(NAME ui-startup COMMAND headset-controller --smoke-test)
    headset_ui_test(ui-startup)
    add_test(NAME cli COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.py" $<TARGET_FILE:headset-controller>)
    set_tests_properties(cli PROPERTIES TIMEOUT 60)
endif()
if(HEADSET_BENCHMARKS)
    qt_add_executable(discovery-benchmark tests/discovery_benchmark.cpp)
    target_link_libraries(discovery-benchmark PRIVATE headset-core)
    headset_target_options(discovery-benchmark)
endif()

include(GNUInstallDirs)
install(TARGETS headset-controller RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(HEADSET_DEPLOY_QT)
    qt_generate_deploy_qml_app_script(TARGET headset-controller OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS INCLUDE_PLUGINS qoffscreen qwayland qwayland-egl)
    install(SCRIPT ${deploy_script})
endif()
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/headset-controller)
install(FILES packaging/headset-controller.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/headset-controller.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
set(CPACK_PACKAGE_VENDOR "Headset Controller contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Read-only Linux headset discovery and diagnostics")
set(CPACK_GENERATOR "TGZ")
include(CPack)
