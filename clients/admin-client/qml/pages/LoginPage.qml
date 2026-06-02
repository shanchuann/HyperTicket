import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    anchors.fill: parent

    Rectangle {
        anchors.centerIn: parent
        width: 400
        height: loginColumn.implicitHeight + 64
        color: Theme.colorScheme.surfaceVariant
        radius: 12

        ColumnLayout {
            id: loginColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
            spacing: 20

            // 标题
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "HyperTicket"
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: Theme.colorScheme.primary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "管理员登录"
                    font.pixelSize: 14
                    color: Theme.colorScheme.onSurfaceVariant
                }
            }

            // 错误提示
            Rectangle {
                Layout.fillWidth: true
                height: 40
                color: Theme.colorScheme.errorContainer
                radius: 4
                visible: errorText.text !== ""
                Text {
                    id: errorText
                    anchors { fill: parent; margins: 10 }
                    wrapMode: Text.WordWrap
                    color: Theme.colorScheme.onErrorContainer
                    font.pixelSize: 13
                }
            }

            // 账号
            TextField {
                id: usernameField
                Layout.fillWidth: true
                placeholderText: "管理员账号"
                onAccepted: passwordField.forceActiveFocus()
            }

            // 密码
            TextField {
                id: passwordField
                Layout.fillWidth: true
                placeholderText: "密码"
                echoMode: TextInput.Password
                onAccepted: doLogin()
            }

            // 登录按钮
            Button {
                Layout.fillWidth: true
                text: busy ? "验证中..." : "登录"
                enabled: !busy && usernameField.text.length > 0 && passwordField.text.length > 0
                property bool busy: false

                onClicked: doLogin()

                function doLogin() {
                    busy = true
                    errorText.text = ""
                    var payload = {
                        "type": 8,
                        "username": usernameField.text,
                        "passward": passwordField.text
                    }
                    tcpClient.request(JSON.stringify(payload), function(jsonStr) { var resp = JSON.parse(jsonStr)
                        busy = false
                        if (resp.status === "OK") {
                            app.adminToken = resp.admin_token || ""
                            app.adminUsername = resp.username || ""
                            app.adminRole = resp.role || ""
                            app.currentPage = 0
                        } else {
                            var msgs = {
                                "ADMIN_INVALID_CREDENTIALS": "账号或密码错误",
                                "DB_UNAVAILABLE": "服务暂时不可用",
                                "RATE_LIMITED": "操作过于频繁"
                            }
                            errorText.text = msgs[resp.reason] || (resp.reason || "登录失败")
                        }
                    })
                }
            }
        }
    }
}
