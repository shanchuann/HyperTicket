import QtQuick
import QtQuick.Layouts
import md3.Core

Card {
    property string label: ""
    property int value: 0
    property color accentColor: Theme.colorScheme.primary
    property string iconName: ""

    padding: 20

    RowLayout {
        spacing: 16
        Rectangle {
            width: 48; height: 48; radius: 12
            color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.12)
            Text {
                anchors.centerIn: parent
                text: iconName
                font.family: "Material Icons Round"
                font.pixelSize: 24
                color: accentColor
            }
        }
        ColumnLayout {
            spacing: 4
            Text { text: label; font.pixelSize: 13; color: Theme.colorScheme.onSurfaceVariant }
            Text { text: value.toLocaleString(); font.pixelSize: 28; font.weight: Font.Bold; color: Theme.colorScheme.onSurface }
        }
    }
}
