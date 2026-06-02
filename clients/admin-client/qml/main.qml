import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    title: "HyperTicket 管理端"

    // MD3 主题配置
    Theme {
        id: theme
        colorSchemeName: "default"
    }

    background: Rectangle { color: Theme.colorScheme.background }

    Loader {
        id: pageLoader
        anchors.fill: parent
        source: "App.qml"
    }
}
