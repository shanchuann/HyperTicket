import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    id: app
    anchors.fill: parent

    // 管理员 token，登录后设置
    property string adminToken: ""
    property string adminUsername: ""
    property string adminRole: ""

    // 当前页面索引（登录时=-1）
    property int currentPage: -1

    readonly property var pageLabels: ["概览", "票务管理", "用户管理"]
    readonly property var pageIcons: ["assessment", "confirmation_number", "group"]
    readonly property var pageSources: [
        "pages/DashboardPage.qml",
        "pages/TicketManagePage.qml",
        "pages/UserManagePage.qml"
    ]

    // 登录页
    Loader {
        id: loginLoader
        anchors.fill: parent
        source: "pages/LoginPage.qml"
        visible: app.currentPage < 0
        active: app.currentPage < 0
    }

    // 主界面（侧边栏 + 内容区）
    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: app.currentPage >= 0

        // MD3 NavigationRail
        NavigationRail {
            id: navRail
            Layout.fillHeight: true
            Layout.preferredWidth: 80
            currentIndex: Math.max(app.currentPage, 0)

            header: Item {
                width: 80
                height: 72
                Text {
                    anchors.centerIn: parent
                    text: "HT"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: Theme.colorScheme.primary
                }
            }

            Repeater {
                model: app.pageLabels
                NavigationRailItem {
                    icon: app.pageIcons[index]
                    label: modelData
                    onClicked: app.currentPage = index
                }
            }

            footer: Item {
                width: 80
                height: 56
                IconButton {
                    anchors.centerIn: parent
                    icon: "logout"
                    onClicked: {
                        app.adminToken = ""
                        app.adminUsername = ""
                        app.currentPage = -1
                    }
                }
            }
        }

        // 内容区
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.colorScheme.surface

            Loader {
                id: contentLoader
                anchors.fill: parent
                anchors.margins: 24
                source: app.currentPage >= 0 ? app.pageSources[app.currentPage] : ""
            }
        }
    }

    // 连接错误提示
    Snackbar {
        id: errorSnack
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
    }

    function showError(msg) {
        errorSnack.message = msg
        errorSnack.open()
    }
}
