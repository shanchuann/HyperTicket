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
        loading = true
        errorMsg = ""
        // 先加载统计
        tcpClient.request(JSON.stringify({ "type": 13, "admin_token": app.adminToken }), function(jsonStr) { var resp = JSON.parse(jsonStr)
            if (resp.status === "OK") {
                stats = { user_count: resp.user_count||0, ticket_count: resp.ticket_count||0,
                          order_count: resp.order_count||0, today_orders: resp.today_orders||0 }
            }
            // 再加载票务列表
            tcpClient.request(JSON.stringify({ "type": 9, "admin_token": app.adminToken }), function(s2) {
                var resp2 = JSON.parse(s2)
                loading = false
                if (resp2.status === "OK") {
                    tickets = resp2.arr || []
                } else {
                    errorMsg = resp2.reason || "加载失败"
                }
            })
        })
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: 24

        Text {
            text: "管理概览"
            font.pixelSize: 28
            font.weight: Font.Bold
            color: Theme.colorScheme.onBackground
        }

        // 统计卡片
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 16
            columnSpacing: 16

            Repeater {
                model: [
                    { label: "在售票务", value: stats.ticket_count, icon: "confirmation_number", color: Theme.colorScheme.primary },
                    { label: "注册用户", value: stats.user_count,   icon: "group",               color: Theme.colorScheme.secondary },
                    { label: "累计订单", value: stats.order_count,  icon: "shopping_cart",        color: Theme.colorScheme.tertiary },
                    { label: "今日订单", value: stats.today_orders, icon: "trending_up",          color: Theme.colorScheme.error },
                ]

                Card {
                    Layout.fillWidth: true
                    padding: 20

                    RowLayout {
                        spacing: 16
                        Rectangle {
                            width: 48; height: 48; radius: 12
                            color: Qt.rgba(modelData.color.r, modelData.color.g, modelData.color.b, 0.12)
                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                font.family: "Material Icons Round"
                                font.pixelSize: 24
                                color: modelData.color
                            }
                        }
                        ColumnLayout {
                            spacing: 4
                            Text { text: modelData.label; font.pixelSize: 13; color: Theme.colorScheme.onSurfaceVariant }
                            Text { text: modelData.value.toLocaleString(); font.pixelSize: 28; font.weight: Font.Bold; color: Theme.colorScheme.onSurface }
                        }
                    }
                }
            }
        }

        // 票务列表
        Card {
            Layout.fillWidth: true
            padding: 20

            ColumnLayout {
                width: parent.width
                spacing: 16

                Text { text: "票务列表"; font.pixelSize: 18; font.weight: Font.SemiBold; color: Theme.colorScheme.onSurface }

                // loading
                CircularProgress { visible: loading; anchors.horizontalCenter: parent.horizontalCenter }

                // error
                Text { visible: errorMsg !== ""; text: errorMsg; color: Theme.colorScheme.error; font.pixelSize: 13 }

                DataTable {
                    id: ticketTable
                    Layout.fillWidth: true
                    visible: !loading && errorMsg === ""
                    model: tickets
                    columns: [
                        { title: "ID",   field: "ticket_id",       width: 60 },
                        { title: "名称", field: "title",           width: -1 },
                        { title: "场馆", field: "venue",           width: 160 },
                        { title: "日期", field: "event_date",      width: 120 },
                        { title: "剩余/总量", field: "_stock",     width: 120 },
                        { title: "状态", field: "_statusLabel",    width: 80 },
                    ]
                    delegate: TableRow {
                        rowData: {
                            "ticket_id": modelData.ticket_id,
                            "title": modelData.title,
                            "venue": modelData.venue,
                            "event_date": modelData.event_date,
                            "_stock": modelData.available_seats + " / " + modelData.total_seats,
                            "_statusLabel": modelData.status === 1 ? "在售" : "下架"
                        }
                    }
                }
            }
        }
    }
}
