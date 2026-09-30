import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import LearningBox

// ★ toggle for a card: filled = in Favorite words. Set `cardId`; the star follows CardStore.
ToolButton {
    id: root
    property int cardId: -1
    // Updated explicitly on favoritesChanged: a binding that only "reads" favoriteCount to depend on it
    // can be optimised away by the QML compiler, and then the star never changes.
    property bool starred: false
    function refresh() { starred = cardId >= 0 && CardStore.isFavorite(cardId) }
    onCardIdChanged: refresh()
    Component.onCompleted: refresh()
    Connections {
        target: CardStore
        function onFavoritesChanged() { root.refresh() }
    }

    focusPolicy: Qt.NoFocus
    enabled: cardId >= 0
    implicitWidth: 40
    contentItem: Item {
        implicitWidth: 24
        implicitHeight: 24
        StarIcon {
            anchors.centerIn: parent
            width: 24
            height: 24
            filled: root.starred
            color: root.starred ? "#F5B301" : Material.foreground
            opacity: root.starred ? 1 : 0.6
        }
    }
    ToolTip.visible: hovered
    ToolTip.text: starred ? qsTr("Remove from Favorite words") : qsTr("Add to Favorite words")
    onClicked: CardStore.setFavorite(cardId, !starred)
}
