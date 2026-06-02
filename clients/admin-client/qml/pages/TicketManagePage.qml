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
        tcpClient.request(JSON.stringify({ "type": 9, "admin_token": app.adminToken }), function(jsonStr) {
            var resp = JSON.parse(jsonStr)
            loading = false
            if (resp.status === "OK") tickets = resp.arr || []
            else errorMsg = resp.reason || "加载失败"
        })
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 20

        RowLayout {
            Layout.fillWidth: true
            Text { text: "票务管理"; font.pixelSize: 28; font.bold: true; color: Theme.color.onBackgroundColor }
            Item { Layout.fillWidth: true }
            Button { text: "添加票务"; icon: "add"; onClicked: addDialog.open() }
        }

        CircularProgress { visible: loading; indeterminate: true; Layout.alignment: Qt.AlignHCenter }
        Text { visible: errorMsg !== ""; text: "加载失败：" + errorMsg; color: Theme.color.error }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth
            Column {
                width: parent.width; spacing: 12
                Repeater {
                    model: tickets
                    Card {
                        width: parent.width; padding: 16
                        RowLayout {
                            width: parent.width; spacing: 16
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: modelData.title; font.pixelSize: 16; font.bold: true; color: Theme.color.onSurfaceColor }
                                Text { text: modelData.venue + "  ·  " + modelData.event_date; font.pixelSize: 13; color: Theme.color.onSurfaceVariantColor }
                                Text { text: "剩余 " + modelData.available_seats + " / " + modelData.total_seats + " 张"; font.pixelSize: 13; color: Theme.color.secondary }
                            }
                            Text {
                                text: modelData.status === 1 ? "在售" : "下架"
                                color: modelData.status === 1 ? Theme.color.tertiary : Theme.color.error
                                font.pixelSize: 13
                            }
                            Button {
                                text: "下架"; type: "outlined"
                                visible: modelData.status === 1
                                onClicked: { deleteDialog.ticketId = modelData.ticket_id; deleteDialog.ticketTitle = modelData.title; deleteDialog.open() }
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
        acceptText: "确认添加"
        rejectText: "取消"
        anchors.centerIn: parent
        width: 400

        property string addError: ""

        text: addError

        Column {
            width: parent.width; spacing: 12
            TextField { id: titleField; placeholderText: "票务名称"; width: parent.width }
            TextField { id: venueField; placeholderText: "场馆"; width: parent.width }
            TextField { id: dateField;  placeholderText: "演出日期 (YYYY-MM-DD)"; width: parent.width }
            TextField { id: seatsField; placeholderText: "总座位数"; width: parent.width }
        }

        onAccepted: {
            addDialog.addError = ""
            var seats = parseInt(seatsField.text)
            if (!titleField.text || !venueField.text || !dateField.text) { addDialog.addError = "请填写完整信息"; addDialog.open(); return }
            if (isNaN(seats) || seats <= 0) { addDialog.addError = "座位数必须为正整数"; addDialog.open(); return }
            tcpClient.request(JSON.stringify({
                "type": 10, "admin_token": app.adminToken,
                "title": titleField.text, "venue": venueField.text,
                "event_date": dateField.text, "total_seats": seats
            }), function(s) {
                var resp = JSON.parse(s)
                if (resp.status === "OK") { loadTickets(); titleField.text = venueField.text = dateField.text = seatsField.text = "" }
                else { addDialog.addError = resp.reason || "添加失败"; addDialog.open() }
            })
        }
        onRejected: { titleField.text = venueField.text = dateField.text = seatsField.text = ""; addDialog.addError = "" }
    }

    // 下架确认对话框
    Dialog {
        id: deleteDialog
        title: "确认下架"
        acceptText: "确认下架"
        rejectText: "取消"
        anchors.centerIn: parent
        property int ticketId: 0
        property string ticketTitle: ""
        text: "确认下架「" + ticketTitle + "」？此操作不可撤销。"

        onAccepted: {
            tcpClient.request(JSON.stringify({ "type": 11, "admin_token": app.adminToken, "ticket_id": ticketId }), function(s) {
                var resp = JSON.parse(s)
                if (resp.status === "OK") loadTickets()
                else app.showError(resp.reason || "下架失败")
            })
        }
    }
}
