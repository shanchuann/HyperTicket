import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

Item {
    id: root
    property var tickets: []
    property bool loading: true
    property string errorMsg: ""

    Component.onCompleted: loadTickets()

    function loadTickets() {
        loading = true; errorMsg = ""
        tcpClient.request(JSON.stringify({ "type": 9, "admin_token": app.adminToken }), function(jsonStr) { var resp = JSON.parse(jsonStr)
            loading = false
            if (resp.status === "OK") tickets = resp.arr || []
            else errorMsg = resp.reason || "加载失败"
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        // Header
        RowLayout {
            Layout.fillWidth: true
            Text { text: "票务管理"; font.pixelSize: 28; font.weight: Font.Bold; color: Theme.colorScheme.onBackground }
            Item { Layout.fillWidth: true }
            Button {
                text: "添加票务"
                icon.name: "add"
                onClicked: addDialog.open()
            }
        }

        CircularProgress { visible: loading; Layout.alignment: Qt.AlignHCenter }
        Text { visible: errorMsg !== ""; text: "加载失败：" + errorMsg; color: Theme.colorScheme.error }

        // 票务卡片列表
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            Column {
                width: parent.width
                spacing: 12

                Repeater {
                    model: tickets
                    Card {
                        width: parent.width
                        padding: 16

                        RowLayout {
                            width: parent.width
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: modelData.title; font.pixelSize: 16; font.weight: Font.Medium; color: Theme.colorScheme.onSurface }
                                Text { text: modelData.venue + "  ·  " + modelData.event_date; font.pixelSize: 13; color: Theme.colorScheme.onSurfaceVariant }
                                Text { text: "剩余 " + modelData.available_seats + " / " + modelData.total_seats + " 张"; font.pixelSize: 13; color: Theme.colorScheme.secondary }
                            }

                            Chip {
                                text: modelData.status === 1 ? "在售" : "下架"
                                color: modelData.status === 1 ? Theme.colorScheme.tertiaryContainer : Theme.colorScheme.errorContainer
                            }

                            IconButton {
                                icon: "delete"
                                enabled: modelData.status === 1
                                onClicked: {
                                    deleteDialog.ticketId = modelData.ticket_id
                                    deleteDialog.ticketTitle = modelData.title
                                    deleteDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 添加票务对话框
    Dialog {
        id: addDialog
        title: "添加票务"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: parent
        width: 420

        property bool submitting: false

        ColumnLayout {
            width: parent.width
            spacing: 16
            TextField { id: titleField;     Layout.fillWidth: true; placeholderText: "票务名称" }
            TextField { id: venueField;     Layout.fillWidth: true; placeholderText: "场馆" }
            TextField { id: dateField;      Layout.fillWidth: true; placeholderText: "演出日期 (YYYY-MM-DD)" }
            TextField { id: seatsField;     Layout.fillWidth: true; placeholderText: "总座位数"; inputMethodHints: Qt.ImhDigitsOnly }
            Text { id: addError; color: Theme.colorScheme.error; font.pixelSize: 13; visible: text !== "" }
        }

        onAccepted: {
            addError.text = ""
            var seats = parseInt(seatsField.text)
            if (!titleField.text || !venueField.text || !dateField.text) { addError.text = "请填写完整信息"; open(); return }
            if (isNaN(seats) || seats <= 0) { addError.text = "座位数必须为正整数"; open(); return }
            submitting = true
            var addPayload = {
                "type": 10, "admin_token": app.adminToken,
                "title": titleField.text, "venue": venueField.text,
                "event_date": dateField.text, "total_seats": seats
            }
            tcpClient.request(JSON.stringify(addPayload), function(s) {
                var resp = JSON.parse(s)
                submitting = false
                if (resp.status === "OK") { loadTickets(); titleField.text = venueField.text = dateField.text = seatsField.text = "" }
                else { addError.text = resp.reason || "添加失败"; open() }
            })
        }
    }

    // 下架确认对话框
    Dialog {
        id: deleteDialog
        title: "确认下架"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: parent
        property int ticketId: 0
        property string ticketTitle: ""

        Text {
            text: "确认下架「" + deleteDialog.ticketTitle + "」？此操作不可撤销。"
            wrapMode: Text.WordWrap
            color: Theme.colorScheme.onSurface
            font.pixelSize: 14
        }

        onAccepted: {
            tcpClient.request(JSON.stringify({ "type": 11, "admin_token": app.adminToken, "ticket_id": ticketId }), function(jsonStr) { var resp = JSON.parse(jsonStr)
                if (resp.status === "OK") loadTickets()
                else app.showError(resp.reason || "下架失败")
            })
        }
    }
}
