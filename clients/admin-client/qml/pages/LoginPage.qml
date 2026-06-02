import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    anchors.fill: parent

    // 监听 connectionError 信号（连接超时/拒绝时的保险）
    Connections {
        target: tcpClient
        function onConnectionError(msg) {
            loginBtn.busy = false
            errorText.text = "连接失败：" + msg
        }
    }

    Card {
        anchors.centerIn: parent
        width: 400

        ColumnLayout {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
            spacing: 20

            // 标题
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "HyperTicket"
                    font.pixelSize: 28; font.bold: true
                    color: Theme.color.primary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "管理员登录"
                    font.pixelSize: 14
                    color: Theme.color.onSurfaceVariantColor
                }
            }

            // 错误提示
            Rectangle {
                Layout.fillWidth: true
                height: errorText.implicitHeight + 16
                color: "#1Cff0000"
                radius: 4
                visible: errorText.text !== ""
                Text {
                    id: errorText
                    anchors { fill: parent; margins: 8 }
                    wrapMode: Text.WordWrap
                    color: Theme.color.error
                    font.pixelSize: 13
                }
            }

            TextField {
                id: usernameField
                label: "管理员账号"
                placeholderText: "请输入账号"
                Layout.fillWidth: true
            }

            TextField {
                id: passwordField
                label: "密码"
                placeholderText: "请输入密码"
                isPassword: true
                Layout.fillWidth: true
            }

            Button {
                id: loginBtn
                Layout.fillWidth: true
                Layout.bottomMargin: 0
                text: busy ? "验证中..." : "登录管理后台"
                enabled: !busy && usernameField.text.length > 0 && passwordField.text.length > 0
                property bool busy: false

                onClicked: {
                    busy = true
                    errorText.text = ""
                    tcpClient.request(
                        JSON.stringify({
                            "type": 8,
                            "username": usernameField.text,
                            "passward": passwordField.text
                        }),
                        function(jsonStr) {
                            loginBtn.busy = false
                            var resp = JSON.parse(jsonStr)
                            if (resp.status === "OK") {
                                app.adminToken    = resp.admin_token || ""
                                app.adminUsername = resp.username    || ""
                                app.adminRole     = resp.role        || ""
                                app.currentPage   = 0
                            } else {
                                var msgs = {
                                    "ADMIN_INVALID_CREDENTIALS": "账号或密码错误",
                                    "DB_UNAVAILABLE":            "服务暂时不可用",
                                    "RATE_LIMITED":              "操作过于频繁"
                                }
                                errorText.text = msgs[resp.reason] || resp.reason || "登录失败"
                            }
                        }
                    )
                }
            }

            // 底部间距
            Item { height: 8 }
        }
    }
}
