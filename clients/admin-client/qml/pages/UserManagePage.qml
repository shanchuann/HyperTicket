import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    id: root
    property var users: []
    property bool loading: true
    property string searchText: ""

    Component.onCompleted: loadUsers()

    function loadUsers() {
        loading = true
        tcpClient.request(JSON.stringify({ "type": 12, "admin_token": app.adminToken }), function(jsonStr) {
            var resp = JSON.parse(jsonStr)
            loading = false
            if (resp.status === "OK") users = resp.arr || []
            else app.showError(resp.reason || "加载失败")
        })
    }

    function maskTel(tel) {
        return tel.length === 11 ? tel.slice(0,3) + "****" + tel.slice(7) : tel
    }

    property var filteredUsers: users.filter(function(u) {
        if (searchText === "") return true
        return u.username.toLowerCase().includes(searchText.toLowerCase()) || u.tel.includes(searchText)
    })

    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            Text { text: "用户管理"; font.pixelSize: 28; font.bold: true; color: Theme.color.onBackgroundColor }
            Item { Layout.fillWidth: true }
            TextField {
                placeholderText: "搜索用户名或手机号"
                text: root.searchText
                onTextChanged: root.searchText = text
                implicitWidth: 240
            }
        }

        CircularProgress { visible: loading; indeterminate: true; Layout.alignment: Qt.AlignHCenter }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
            Column {
                width: parent.width; spacing: 8
                Repeater {
                    model: filteredUsers
                    Card {
                        width: parent.width; padding: 14
                        RowLayout {
                            width: parent.width; spacing: 16
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: modelData.status === 1 ? Theme.color.tertiary : Theme.color.error
                            }
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                Text { text: modelData.username; font.pixelSize: 15; font.bold: true; color: Theme.color.onSurfaceColor }
                                Text { text: maskTel(modelData.tel); font.pixelSize: 13; color: Theme.color.onSurfaceVariantColor }
                            }
                            Text {
                                text: modelData.status === 1 ? "正常" : "黑名单"
                                color: modelData.status === 1 ? Theme.color.tertiary : Theme.color.error
                                font.pixelSize: 13
                            }
                            Button {
                                text: modelData.status === 1 ? "封禁" : "解封"
                                type: modelData.status === 1 ? "outlined" : "filled"
                                onClicked: {
                                    var action = modelData.status === 1 ? "add" : "remove"
                                    var blPayload = { "type": 14, "admin_token": app.adminToken,
                                                      "tel": modelData.tel, "action": action }
                                    tcpClient.request(JSON.stringify(blPayload), function(s) {
                                        var resp = JSON.parse(s)
                                        if (resp.status === "OK") loadUsers()
                                        else app.showError(resp.reason || "操作失败")
                                    })
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
