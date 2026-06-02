cmake_minimum_required(VERSION 3.16)
project(HyperTicketAdmin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS Quick Gui Svg Network Widgets)
qt_standard_project_setup(REQUIRES 6.4)

# MD3 组件库
add_subdirectory(material-components-qml)

qt_add_executable(HyperTicketAdmin
    src/main.cpp
    src/TcpClient.cpp
)

qt_add_qml_module(HyperTicketAdmin
    URI HyperTicketAdmin
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/App.qml
        qml/pages/LoginPage.qml
        qml/pages/DashboardPage.qml
        qml/pages/TicketManagePage.qml
        qml/pages/UserManagePage.qml
        qml/components/StatCard.qml
        qml/components/StockBar.qml
)

target_include_directories(HyperTicketAdmin PRIVATE src)

target_link_libraries(HyperTicketAdmin PRIVATE
    Qt6::Quick
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
    Qt6::Widgets
    md3Core
)
