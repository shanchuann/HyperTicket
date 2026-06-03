import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    anchors.fill: parent

    // 监听连接错误（保险）
    Connections {
        target: tcpClient
        function onConnectionError(msg) {
            loginBtn.busy = false
            errorText.text = "连接失败：" + msg
        }
    }

    // 背景
    Rectangle {
        anchors.fill: parent
        color: Theme.color.background
    }

    // 登录卡片，精确垂直居中
    Rectangle {
        id: loginCard
        anchors.centerIn: parent
        width: 420
        height: formColumn.implicitHeight + 64  // 上下各 32px 内边距
        radius: 16
        color: Theme.color.surfaceContainerHigh || Theme.color.surface

        // 内容列
        Column {
            id: formColumn
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                margins: 32
            }
            spacing: 20

            // 标题区
            Column {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "HyperTicket"
                    font.pixelSize: 28; font.bold: true
                    color: Theme.color.primary
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "管理员登录"
                    font.pixelSize: 14
                    color: Theme.color.onSurfaceVariantColor
                }
            }

            // 错误提示框
            Rectangle {
                width: parent.width
                height: errorText.implicitHeight + 16
                radius: 8
                color: Qt.rgba(
                    Theme.color.error.r,
                    Theme.color.error.g,
                    Theme.color.error.b, 0.12)
                visible: errorText.text !== ""

                Text {
                    id: errorText
                    anchors { fill: parent; margins: 8 }
                    wrapMode: Text.WordWrap
                    color: Theme.color.error
                    font.pixelSize: 13
                }
            }

            // 账号输入
            TextField {
                id: usernameField
                label: "管理员账号"
                placeholderText: "请输入账号"
                width: parent.width
            }

            // 密码输入
            TextField {
                id: passwordField
                label: "密码"
                placeholderText: "请输入密码"
                isPassword: true
                width: parent.width
            }

            // 登录按钮
            Button {
                id: loginBtn
                width: parent.width
                text: busy ? "验证中..." : "登录管理后台"
                type: "filled"
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
            Item { height: 0 }
        }
    }
}
