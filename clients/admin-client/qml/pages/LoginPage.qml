import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    anchors.fill: parent

    Card {
        anchors.centerIn: parent
        width: 380; padding: 32

        ColumnLayout {
            width: parent.width; spacing: 20

            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Text { Layout.alignment: Qt.AlignHCenter; text: "HyperTicket"; font.pixelSize: 28; font.bold: true; color: Theme.color.primary }
                Text { Layout.alignment: Qt.AlignHCenter; text: "管理员登录"; font.pixelSize: 14; color: Theme.color.onSurfaceVariantColor }
            }

            Text { id: errorText; color: Theme.color.error; font.pixelSize: 13; visible: text !== ""; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            TextField { id: usernameField; label: "管理员账号"; placeholderText: "请输入账号"; Layout.fillWidth: true }
            TextField { id: passwordField; label: "密码"; placeholderText: "请输入密码"; isPassword: true; Layout.fillWidth: true }

            Button {
                Layout.fillWidth: true
                text: busy ? "验证中..." : "登录管理后台"
                enabled: !busy && usernameField.text.length > 0 && passwordField.text.length > 0
                property bool busy: false

                onClicked: doLogin()

                function doLogin() {
                    busy = true; errorText.text = ""
                    tcpClient.request(JSON.stringify({ "type": 8, "username": usernameField.text, "passward": passwordField.text }), function(jsonStr) {
                        var resp = JSON.parse(jsonStr)
                        busy = false
                        if (resp.status === "OK") {
                            app.adminToken = resp.admin_token || ""
                            app.adminUsername = resp.username || ""
                            app.adminRole = resp.role || ""
                            app.currentPage = 0
                        } else {
                            var msgs = { "ADMIN_INVALID_CREDENTIALS": "账号或密码错误", "DB_UNAVAILABLE": "服务暂时不可用", "RATE_LIMITED": "操作过于频繁" }
                            errorText.text = msgs[resp.reason] || (resp.reason || "登录失败")
                        }
                    })
                }
            }
        }
    }
}
