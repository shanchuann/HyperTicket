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

    Loader {
        anchors.fill: parent
        source: "pages/LoginPage.qml"
        visible: app.currentPage < 0
        active:  app.currentPage < 0
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: app.currentPage >= 0

        NavigationRail {
            id: navRail
            Layout.fillHeight: true
            currentIndex: Math.max(app.currentPage, 0)
            model: [
                { icon: "assessment",          text: "概览"     },
                { icon: "confirmation_number", text: "票务管理" },
                { icon: "group",               text: "用户管理" }
            ]
            onItemClicked: function(index) { app.currentPage = index }

            header: Item {
                width: navRail.implicitWidth; height: 64
                Text {
                    anchors.centerIn: parent
                    text: "HT"; font.pixelSize: 22; font.bold: true
                    color: Theme.color.primary
                }
            }

            footer: IconButton {
                icon: "logout"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    app.adminToken = ""; app.adminUsername = ""; app.currentPage = -1
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: Theme.color.background
            Loader {
                anchors.fill: parent; anchors.margins: 24
                source: app.currentPage >= 0 ? app.pageSources[app.currentPage] : ""
            }
        }
    }

    Snackbar {
        id: errorSnack
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
    }

    function showError(msg) { errorSnack.text = msg; errorSnack.open() }
}
