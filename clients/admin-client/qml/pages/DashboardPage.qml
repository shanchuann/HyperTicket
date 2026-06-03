import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import md3.Core

ScrollView {
    id: root
    contentWidth: availableWidth

    property var stats: ({ user_count:0, ticket_count:0, order_count:0, today_orders:0 })
    property var tickets: []
    property bool loading: true
    property string errorMsg: ""

    Component.onCompleted: loadData()

    function loadData() {
        loading = true; errorMsg = ""
        tcpClient.request(JSON.stringify({ "type": 13, "admin_token": app.adminToken }), function(jsonStr) {
            var resp = JSON.parse(jsonStr)
            if (resp.status === "OK") {
                stats = { user_count: resp.user_count||0, ticket_count: resp.ticket_count||0,
                          order_count: resp.order_count||0, today_orders: resp.today_orders||0 }
            }
            tcpClient.request(JSON.stringify({ "type": 9, "admin_token": app.adminToken }), function(s2) {
                var resp2 = JSON.parse(s2)
                loading = false
                if (resp2.status === "OK") tickets = resp2.arr || []
                else errorMsg = resp2.reason || "加载失败"
            })
        })
    }

    ColumnLayout {
        width: root.availableWidth
        anchors.margins: 24
        spacing: 24

        // 统计卡片
        GridLayout {
            Layout.fillWidth: true
            columns: 4; rowSpacing: 16; columnSpacing: 16

            Repeater {
                model: [
                    { label: "在售票务", value: stats.ticket_count, accent: Theme.color.primary },
                    { label: "注册用户", value: stats.user_count,   accent: Theme.color.secondary },
                    { label: "累计订单", value: stats.order_count,  accent: Theme.color.tertiary },
                    { label: "今日订单", value: stats.today_orders, accent: Theme.color.error },
                ]
                Card {
                    Layout.fillWidth: true; padding: 20
                    ColumnLayout {
                        spacing: 4
                        Text { text: modelData.label; font.pixelSize: 13; color: Theme.color.onSurfaceVariantColor }
                        Text {
                            text: modelData.value.toLocaleString("zh-CN")
                            font.pixelSize: 28; font.bold: true
                            color: modelData.accent
                        }
                    }
                }
            }
        }

        // 票务列表
        Card {
            Layout.fillWidth: true; padding: 20
            ColumnLayout {
                width: parent.width; spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "票务列表"; font.pixelSize: 18; font.bold: true; color: Theme.color.onSurfaceColor }
                    Item { Layout.fillWidth: true }
                    Button { text: "刷新"; onClicked: loadData() }
                }

                CircularProgress {
                    visible: loading; indeterminate: true
                    Layout.alignment: Qt.AlignHCenter
                }
                Text { visible: errorMsg !== ""; text: errorMsg; color: Theme.color.error; font.pixelSize: 13 }

                // DataTable 使用 role (不是 field)
                DataTable {
                    Layout.fillWidth: true
                    visible: !loading && errorMsg === ""
                    columns: [
                        { label: "ID",      role: "id_str",    width: 70  },
                        { label: "名称",    role: "title",     width: -1  },
                        { label: "场馆",    role: "venue",     width: 160 },
                        { label: "日期",    role: "event_date",width: 120 },
                        { label: "剩余/总", role: "stock",     width: 130 },
                        { label: "状态",    role: "status_str",width: 80  },
                    ]
                    rowData: tickets.map(function(t) {
                        return {
                            id_str:     String(t.ticket_id),
                            title:      String(t.title      || ""),
                            venue:      String(t.venue      || ""),
                            event_date: String(t.event_date || ""),
                            stock:      String(t.available_seats) + " / " + String(t.total_seats),
                            status_str: t.status === 1 ? "在售" : "下架"
                        }
                    })
                }
            }
        }
    }
}
