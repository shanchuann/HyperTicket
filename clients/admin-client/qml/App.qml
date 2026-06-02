import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    id: app
    anchors.fill: parent

    property string adminToken: ""
    property string adminUsername: ""
    property string adminRole: ""
    property int currentPage: -1

    readonly property var pageSources: [
        "pages/DashboardPage.qml",
        "pages/TicketManagePage.qml",
        "pages/UserManagePage.qml"
    ]

    // ── 登录页 ──────────────────────────────────────────────────
    Loader {
        anchors.fill: parent
        source: "pages/LoginPage.qml"
        visible: app.currentPage < 0
        active:  app.currentPage < 0
    }

    // ── 主界面 ──────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: app.currentPage >= 0

        // 左侧 NavigationRail
        NavigationRail {
            id: navRail
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            currentIndex: Math.max(app.currentPage, 0)
            model: [
                { icon: "assessment",          text: "概览"     },
                { icon: "confirmation_number", text: "票务管理" },
                { icon: "group",               text: "用户管理" }
            ]
            onItemClicked: function(index) { app.currentPage = index }

            // Header：品牌标识
            header: Rectangle {
                width: navRail.implicitWidth; height: 64
                color: "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "HT"
                    font.pixelSize: 20; font.bold: true
                    color: Theme.color.primary
                }
            }

            // Footer：登出
            footer: Rectangle {
                width: navRail.implicitWidth; height: 56
                color: "transparent"
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

        // 右侧内容区（顶部 Toolbar + 页面内容）
        Rectangle {
            anchors {
                left: navRail.right
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            color: Theme.color.background

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 顶部 Toolbar
                Rectangle {
                    Layout.fillWidth: true
                    height: 56
                    color: Theme.color.surface
                    layer.enabled: true

                    RowLayout {
                        anchors { fill: parent; leftMargin: 24; rightMargin: 16 }
                        spacing: 8

                        // 当前页面标题
                        Text {
                            Layout.fillWidth: true
                            text: app.currentPage === 0 ? "管理概览"
                                : app.currentPage === 1 ? "票务管理"
                                : "用户管理"
                            font.pixelSize: 18; font.bold: true
                            color: Theme.color.onSurfaceColor
                            verticalAlignment: Text.AlignVCenter
                        }

                        // 管理员信息
                        Text {
                            text: app.adminRole ? app.adminUsername + " · " + app.adminRole : app.adminUsername
                            font.pixelSize: 13
                            color: Theme.color.onSurfaceVariantColor
                            visible: app.adminUsername !== ""
                        }

                        // 主题切换
                        IconButton {
                            icon: "light_mode"
                            onClicked: {
                                // 切换 MD3 深浅色
                                if (typeof StyleManager !== "undefined")
                                    StyleManager.isDarkTheme = !StyleManager.isDarkTheme
                            }
                        }

                        // 登出
                        IconButton {
                            icon: "logout"
                            onClicked: {
                                app.adminToken = ""; app.adminUsername = ""; app.currentPage = -1
                            }
                        }
                    }
                }

                // 页面内容
                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: app.currentPage >= 0 ? app.pageSources[app.currentPage] : ""

                    // 给内容加内边距
                    Item {
                        anchors.fill: parent
                        // 内容由 Loader 直接填充，边距在各页面内处理
                    }
                }
            }
        }
    }

    // 全局 Snackbar 错误提示
    Snackbar {
        id: errorSnack
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
    }

    function showError(msg) {
        errorSnack.text = msg
        errorSnack.open()
    }
}
