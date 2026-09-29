import QtQuick

// Help "?" in a circle, drawn in `color` (same style as GearIcon).
Item {
    id: icon
    property color color: "white"
    implicitWidth: 24
    implicitHeight: 24
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(icon.width, icon.height) - 2
        height: width
        radius: width / 2
        color: "transparent"
        border.color: icon.color
        border.width: 2
    }
    Text {
        anchors.centerIn: parent
        text: "?"
        color: icon.color
        font.pixelSize: Math.round(Math.min(icon.width, icon.height) * 0.62)
        font.bold: true
    }
}
