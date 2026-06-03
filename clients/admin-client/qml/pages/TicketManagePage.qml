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

    // ── 主内容 ──────────────────────────────────────────────────
    ColumnLayout {
        anchors { fill: parent; margins: 24 }
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            Text { text: "票务管理"; font.pixelSize: 28; font.bold: true; color: Theme.color.onBackgroundColor }
            Item { Layout.fillWidth: true }
            Button { text: "添加票务"; icon: "add"; type: "filled"; onClicked: addOverlay.visible = true }
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
                                onClicked: {
                                    deleteOverlay.ticketId    = modelData.ticket_id
                                    deleteOverlay.ticketTitle = modelData.title
                                    deleteOverlay.visible     = true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 添加票务 Overlay（简单 Rectangle，不用 MD3 Dialog）─────────────
    Rectangle {
        id: addOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.5)
        visible: false
        z: 100

        MouseArea { anchors.fill: parent; onClicked: addOverlay.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: 440; height: addForm.implicitHeight + 64
            radius: 16
            color: Theme.color.surface

            MouseArea { anchors.fill: parent }  // block click-through

            Column {
                id: addForm
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 16

                Text { text: "添加票务"; font.pixelSize: 20; font.bold: true; color: Theme.color.onSurfaceColor }

                Rectangle {
                    width: parent.width; height: addErrText.implicitHeight + 12; radius: 6
                    color: Qt.rgba(Theme.color.error.r, Theme.color.error.g, Theme.color.error.b, 0.12)
                    visible: addErrText.text !== ""
                    Text { id: addErrText; anchors { fill: parent; margins: 6 }
                    wrapMode: Text.WordWrap; color: Theme.color.error; font.pixelSize: 13 }
                }

                TextField { id: titleF;  label: "票务名称";              placeholderText: "如：2026 周杰伦演唱会"; width: parent.width }
                TextField { id: venueF;  label: "场馆";                  placeholderText: "如：国家体育场（鸟巢）"; width: parent.width }
                TextField { id: dateF;   label: "演出日期 (YYYY-MM-DD)"; placeholderText: "2026-08-15";              width: parent.width }
                TextField { id: seatsF;  label: "总座位数";              placeholderText: "如：80000";               width: parent.width }

                RowLayout {
                    width: parent.width; spacing: 12
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "取消"; type: "outlined"
                        onClicked: { addErrText.text = ""; addOverlay.visible = false }
                    }
                    Button {
                        id: addBtn
                        text: busy ? "提交中..." : "确认添加"; type: "filled"
                        enabled: !busy && titleF.text.length > 0 && venueF.text.length > 0 && dateF.text.length > 0 && seatsF.text.length > 0
                        property bool busy: false
                        onClicked: {
                            addErrText.text = ""
                            var seats = parseInt(seatsF.text)
                            if (isNaN(seats) || seats <= 0) { addErrText.text = "座位数必须为正整数"; return }
                            busy = true
                            tcpClient.request(JSON.stringify({
                                "type": 10, "admin_token": app.adminToken,
                                "title": titleF.text, "venue": venueF.text,
                                "event_date": dateF.text, "total_seats": seats
                            }), function(s) {
                                addBtn.busy = false
                                var resp = JSON.parse(s)
                                if (resp.status === "OK") {
                                    addOverlay.visible = false
                                    titleF.text = venueF.text = dateF.text = seatsF.text = ""
                                    loadTickets()
                                } else {
                                    addErrText.text = resp.reason || "添加失败"
                                }
                            })
                        }
                    }
                }
                Item { height: 0 }
            }
        }
    }

    // ── 下架确认 Overlay ────────────────────────────────────────
    Rectangle {
        id: deleteOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.5)
        visible: false
        z: 100
        property int ticketId: 0
        property string ticketTitle: ""

        MouseArea { anchors.fill: parent; onClicked: deleteOverlay.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: 380; height: delForm.implicitHeight + 64
            radius: 16
            color: Theme.color.surface

            MouseArea { anchors.fill: parent }

            Column {
                id: delForm
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 32 }
                spacing: 20

                Text { text: "确认下架"; font.pixelSize: 20; font.bold: true; color: Theme.color.onSurfaceColor }
                Text {
                    width: parent.width
                    text: "确认下架「" + deleteOverlay.ticketTitle + "」？\n此操作不可撤销。"
                    wrapMode: Text.WordWrap
                    color: Theme.color.onSurfaceVariantColor; font.pixelSize: 14
                }

                RowLayout {
                    width: parent.width; spacing: 12
                    Item { Layout.fillWidth: true }
                    Button { text: "取消"; type: "outlined"; onClicked: deleteOverlay.visible = false }
                    Button {
                        text: "确认下架"; type: "filled"
                        onClicked: {
                            deleteOverlay.visible = false
                            tcpClient.request(JSON.stringify({
                                "type": 11, "admin_token": app.adminToken,
                                "ticket_id": deleteOverlay.ticketId
                            }), function(s) {
                                var resp = JSON.parse(s)
                                if (resp.status === "OK") loadTickets()
                                else app.showError(resp.reason || "下架失败")
                            })
                        }
                    }
                }
                Item { height: 0 }
            }
        }
    }
}
