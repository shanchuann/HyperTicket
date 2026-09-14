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
    readonly property var pageSources: ["pages/DashboardPage.qml", "pages/TicketManagePage.qml", "pages/UserManagePage.qml"]
    readonly property var pageTitles: ["运营概览", "票务管理", "用户管理"]
    readonly property var pageIcons: ["assessment", "confirmation_number", "group"]

    Loader { anchors.fill: parent; source: "pages/LoginPage.qml"; visible: app.currentPage < 0; active: app.currentPage < 0 }

    Rectangle {
        anchors.fill: parent
        visible: app.currentPage >= 0
        color: Theme.color.background
        RowLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillHeight: true; Layout.preferredWidth: 224
                color: Theme.color.surfaceContainerLow || Theme.color.surface
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.preferredHeight: 56; spacing: 10
                        Rectangle { Layout.preferredWidth: 36; Layout.preferredHeight: 36; radius: 10; color: Theme.color.primary
                            Text { anchors.centerIn: parent; text: "H"; color: Theme.color.onPrimaryColor; font.pixelSize: 20; font.bold: true }
                        }
                        ColumnLayout { Layout.fillWidth: true; spacing: 0
                            Text { text: "HyperTicket"; color: Theme.color.onSurfaceColor; font.pixelSize: 16; font.bold: true }
                            Text { text: "运营控制台"; color: Theme.color.onSurfaceVariantColor; font.pixelSize: 12 }
                        }
                    }
                    Item { Layout.preferredHeight: 28 }
                    Text { text: "工作区"; color: Theme.color.onSurfaceVariantColor; font.pixelSize: 12; font.bold: true; Layout.leftMargin: 12 }
                    Item { Layout.preferredHeight: 8 }
                    Repeater {
                        model: app.pageTitles
                        delegate: Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 44; radius: 8
                            color: app.currentPage === index ? Theme.color.secondaryContainer : "transparent"
                            RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 12
                                Text { text: app.pageIcons[index]; font.family: Theme.iconFont.name; font.pixelSize: 20; color: app.currentPage === index ? Theme.color.onSecondaryContainerColor : Theme.color.onSurfaceVariantColor }
                                Text { Layout.fillWidth: true; text: modelData; color: app.currentPage === index ? Theme.color.onSecondaryContainerColor : Theme.color.onSurfaceColor; font.pixelSize: 14; font.bold: app.currentPage === index }
                            }
                            MouseArea { anchors.fill: parent; onClicked: app.currentPage = index }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.color.outlineVariant; opacity: 0.45 }
                    Item { Layout.preferredHeight: 12 }
                    RowLayout { Layout.fillWidth: true; spacing: 10
                        Rectangle { Layout.preferredWidth: 32; Layout.preferredHeight: 32; radius: 16; color: Theme.color.tertiaryContainer
                            Text { anchors.centerIn: parent; text: (app.adminUsername || "管").slice(0, 1).toUpperCase(); color: Theme.color.onTertiaryContainerColor; font.bold: true }
                        }
                        ColumnLayout { Layout.fillWidth: true; spacing: 0
                            Text { text: app.adminUsername || "管理员"; color: Theme.color.onSurfaceColor; font.pixelSize: 13; elide: Text.ElideRight }
                            Text { text: app.adminRole || "运营人员"; color: Theme.color.onSurfaceVariantColor; font.pixelSize: 11 }
                        }
                        IconButton { icon: "logout"; onClicked: app.logout() }
                    }
                }
            }
            ColumnLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 72; color: Theme.color.surface
                    RowLayout { anchors.fill: parent; anchors.leftMargin: 28; anchors.rightMargin: 20; spacing: 16
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: app.pageTitles[app.currentPage] || "运营概览"; color: Theme.color.onSurfaceColor; font.pixelSize: 22; font.bold: true }
                            Text { text: app.currentPage === 0 ? "今天的运营状态，一眼掌握" : "管理数据并保持库存与用户状态准确"; color: Theme.color.onSurfaceVariantColor; font.pixelSize: 12 }
                        }
                        Rectangle { width: 1; height: 28; color: Theme.color.outlineVariant; opacity: 0.6 }
                        Text { text: "● 在线"; color: Theme.color.tertiary; font.pixelSize: 13 }
                        IconButton { icon: "light_mode"; onClicked: if (typeof StyleManager !== "undefined") StyleManager.isDarkTheme = !StyleManager.isDarkTheme }
                    }
                }
                Loader { Layout.fillWidth: true; Layout.fillHeight: true; source: app.currentPage >= 0 ? app.pageSources[app.currentPage] : "" }
            }
        }
    }
    Snackbar { id: errorSnack; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 24 }
    function logout() { app.adminToken = ""; app.adminUsername = ""; app.adminRole = ""; app.currentPage = -1 }
    function showError(msg) { errorSnack.text = msg; errorSnack.open() }
}
