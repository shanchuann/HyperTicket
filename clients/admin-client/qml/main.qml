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

    // Theme 是 md3.Core 的 Singleton，直接通过 Theme.xxx 访问，无需实例化
    background: Rectangle { color: Theme.color.background }

    Loader {
        id: pageLoader
        anchors.fill: parent
        source: "App.qml"
    }
}
