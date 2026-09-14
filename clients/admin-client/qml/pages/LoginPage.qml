import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    id: loginPage
    anchors.fill: parent

    // 视图状态：0=登录  1=强制修改密码
    property int viewState: 0

    // 登录后暂存（等修改密码完成再跳转）
    property string pendingToken: ""
    property string pendingUsername: ""
    property string pendingRole: ""

    Connections {
        target: tcpClient
        function onConnectionError(msg) {
            loginBtn.busy = false
            loginErrText.text = "连接失败：" + msg
        }
    }

    // ── 全局背景 ──────────────────────────────────────────────
    Rectangle { anchors.fill: parent; color: Theme.color.background }

    // ═══════════════════════════════════════════════════════════
    // 视图 0：登录
    // ═══════════════════════════════════════════════════════════
    Item {
        anchors.fill: parent
        visible: loginPage.viewState === 0

        Rectangle {
            anchors.centerIn: parent
            width: 420
            height: loginCol.implicitHeight + 64
            radius: 16
            color: Theme.color.surfaceContainerHigh || Theme.color.surface

            Column {
                id: loginCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 20

                Column {
                    width: parent.width; spacing: 8
                    Text {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter
                        text: "HyperTicket"; font.pixelSize: 28; font.bold: true
                        color: Theme.color.primary
                    }
                    Text {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter
                        text: "管理员登录"; font.pixelSize: 14
                        color: Theme.color.onSurfaceVariantColor
                    }
                }

                Rectangle {
                    width: parent.width; height: loginErrText.implicitHeight + 16; radius: 8
                    color: Qt.rgba(Theme.color.error.r, Theme.color.error.g, Theme.color.error.b, 0.12)
                    visible: loginErrText.text !== ""
                    Text {
                        id: loginErrText
                        anchors { fill: parent; margins: 8 }
                        wrapMode: Text.WordWrap; color: Theme.color.error; font.pixelSize: 13
                    }
                }

                TextField { id: usernameField; label: "管理员账号"; placeholderText: "请输入账号"; width: parent.width }
                TextField { id: passwordField; label: "密码"; placeholderText: "请输入密码"; isPassword: true; width: parent.width }

                Button {
                    id: loginBtn
                    width: parent.width; type: "filled"
                    text: busy ? "验证中..." : "登录管理后台"
                    enabled: !busy && usernameField.text.length > 0 && passwordField.text.length > 0
                    property bool busy: false

                    onClicked: {
                        busy = true; loginErrText.text = ""
                        tcpClient.request(
                            JSON.stringify({ "type": 8, "username": usernameField.text, "passward": passwordField.text }),
                            function(jsonStr) {
                                loginBtn.busy = false
                                var resp = JSON.parse(jsonStr)
                                if (resp.status === "OK") {
                                    loginPage.pendingToken    = resp.admin_token || ""
                                    loginPage.pendingUsername = resp.username    || ""
                                    loginPage.pendingRole     = resp.role        || ""
                                    // resp["is_default_password"] 可能是 bool 或数字 1/0
                                    var isDefault = (resp["is_default_password"] === true ||
                                                     resp["is_default_password"] === 1)
                                    if (isDefault) {
                                        loginPage.viewState = 1  // 切换到修改密码视图
                                    } else {
                                        app.adminToken    = loginPage.pendingToken
                                        app.adminUsername = loginPage.pendingUsername
                                        app.adminRole     = loginPage.pendingRole
                                        app.currentPage   = 0
                                    }
                                } else {
                                    var msgs = {
                                        "ADMIN_INVALID_CREDENTIALS": "账号或密码错误",
                                        "DB_UNAVAILABLE":            "服务暂时不可用",
                                        "RATE_LIMITED":              "操作过于频繁"
                                    }
                                    loginErrText.text = msgs[resp.reason] || resp.reason || "登录失败"
                                }
                            }
                        )
                    }
                }
                Item { height: 0 }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════
    // 视图 1：强制修改默认密码
    // ═══════════════════════════════════════════════════════════
    Item {
        anchors.fill: parent
        visible: loginPage.viewState === 1

        Rectangle {
            anchors.centerIn: parent
            width: 420
            height: changePwdCol.implicitHeight + 64
            radius: 16
            color: Theme.color.surfaceContainerHigh || Theme.color.surface

            Column {
                id: changePwdCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 20

                Column {
                    width: parent.width; spacing: 8
                    Text {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter
                        text: "首次登录，请修改密码"; font.pixelSize: 22; font.bold: true
                        color: Theme.color.primary
                    }
                    Rectangle {
                        width: parent.width; height: warnText.implicitHeight + 16; radius: 8
                        color: Qt.rgba(Theme.color.error.r, Theme.color.error.g, Theme.color.error.b, 0.1)
                        Text {
                            id: warnText
                            anchors { fill: parent; margins: 8 }
                            text: "您正在使用初始密码 \"password\"，请立即设置新密码后才能使用系统。"
                            wrapMode: Text.WordWrap; color: Theme.color.error; font.pixelSize: 13
                        }
                    }
                }

                Rectangle {
                    width: parent.width; height: cpErrText.implicitHeight + 16; radius: 8
                    color: Qt.rgba(Theme.color.error.r, Theme.color.error.g, Theme.color.error.b, 0.12)
                    visible: cpErrText.text !== ""
                    Text {
                        id: cpErrText
                        anchors { fill: parent; margins: 8 }
                        wrapMode: Text.WordWrap; color: Theme.color.error; font.pixelSize: 13
                    }
                }

                TextField {
                    id: newPwdField
                    label: "新密码"
                    placeholderText: "6-16位，包含大小写字母和数字"
                    isPassword: true; width: parent.width
                }
                TextField {
                    id: confirmPwdField
                    label: "确认新密码"
                    placeholderText: "再次输入新密码"
                    isPassword: true; width: parent.width
                }

                Button {
                    id: changePwdBtn
                    width: parent.width; type: "filled"
                    text: busy ? "提交中..." : "确认修改并登录"
                    enabled: !busy && newPwdField.text.length >= 6 && confirmPwdField.text.length >= 6
                    property bool busy: false

                    onClicked: {
                        cpErrText.text = ""
                        if (newPwdField.text !== confirmPwdField.text) {
                            cpErrText.text = "两次输入的密码不一致"
                            return
                        }
                        busy = true
                        tcpClient.request(
                            JSON.stringify({ "type": 15, "admin_token": loginPage.pendingToken, "new_password": newPwdField.text }),
                            function(jsonStr) {
                                changePwdBtn.busy = false
                                var resp = JSON.parse(jsonStr)
                                if (resp.status === "OK") {
                                    app.adminToken    = loginPage.pendingToken
                                    app.adminUsername = loginPage.pendingUsername
                                    app.adminRole     = loginPage.pendingRole
                                    app.currentPage   = 0
                                } else {
                                    var msgs = {
                                        "PASSWORD_TOO_WEAK":   "密码强度不足，需包含大写字母、小写字母和数字",
                                        "PASSWORD_SAME_AS_OLD":"不能使用初始密码 \"password\"",
                                        "ADMIN_UNAUTHORIZED":  "登录已过期，请重新登录",
                                        "DB_UNAVAILABLE":      "服务暂时不可用"
                                    }
                                    cpErrText.text = msgs[resp.reason] || resp.reason || "修改失败"
                                }
                            }
                        )
                    }
                }
                Item { height: 0 }
            }
        }
    }
}
