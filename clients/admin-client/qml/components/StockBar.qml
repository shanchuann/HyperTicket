import QtQuick
import md3.Core

Rectangle {
    property real percent: 0
    height: 6
    color: Theme.color.surfaceVariant
    radius: 3

    Rectangle {
        width: parent.width * Math.min(percent / 100, 1)
        height: parent.height
        radius: 3
        color: percent < 10 ? Theme.color.error
             : percent < 30 ? Theme.color.tertiary
             : Theme.color.primary
        Behavior on width { NumberAnimation { duration: 300 } }
    }
}
