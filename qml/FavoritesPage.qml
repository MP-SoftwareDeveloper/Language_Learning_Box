import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

// Favorite words: the starred cards of the learning boxes, newest star first. The language chips filter by language
// (German / English / Swedish): at first only the language of the selected learning box; the others can be switched on.
// Tap a word to edit it; the star removes it from the list (the card itself stays).
Page {
    id: page
    title: qsTr("Favorite words")

    property var all: []        // every favorite, sorted
    property var cards: []      // the ones in the switched-on languages
    property var languages: []  // [{code, name, count}] of the languages that have favorites
    property var shown: ({})    // {code: true}: the switched-on languages; set when the page opens
    property bool filterSet: false
    readonly property var langNames: ({ "de": "Deutsch", "en": "English", "sv": "Svenska" })
    // One tint per language: its chip, and the background of its words
    readonly property var langColors: ({ "de": "#FFB300", "en": "#42A5F5", "sv": "#66BB6A" })
    function langColor(code) { return langColors[code] ?? "#9E9E9E" }

    function applyFilter() {
        cards = all.filter(c => shown[c.language] === true)
    }
    function toggleLanguage(code) {
        const s = Object.assign({}, shown)
        s[code] = !s[code]
        shown = s
        applyFilter()
    }
    function reload() {
        // Keep the scroll position when a star is removed (a new list would jump to the top)
        const y = list.contentY
        all = sortButton.apply(CardStore.favorites())
        const counts = ({})
        for (const c of all)
            counts[c.language] = (counts[c.language] ?? 0) + 1
        languages = ["de", "en", "sv"].filter(l => counts[l] > 0).map(l => ({ code: l, name: langNames[l], count: counts[l] }))
        if (!filterSet) {
            // First: the language of the selected learning box. Without favorites in it: all that have some,
            // so the page is not empty.
            const s = ({})
            const learn = CardStore.learningLanguage
            if (counts[learn] > 0)
                s[learn] = true
            else
                for (const l of languages)
                    s[l.code] = true
            shown = s
            filterSet = all.length > 0
        }
        applyFilter()
        list.forceLayout()
        list.contentY = Math.max(list.originY, Math.min(y, list.originY + list.contentHeight - list.height))
    }
    Component.onCompleted: reload()
    // Rebuilt when cards change or the page is shown again, not for a star: a star tapped here
    // keeps its card in the list (empty star, tap again to undo) until you come back to the page.
    Connections {
        target: CardStore
        function onChanged() { page.reload() }
    }
    StackView.onActivating: reload()
    StackView.onDeactivating: Speaker.stop()

    function boxName(b) { return BoxNames.name(b) }

    Component { id: editPage; CardEditPage {} }

    // Display order only (as added / A-Z / Z-A)
    Item {
        id: sortBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: page.all.length > 0 ? sortButton.implicitHeight + 12 : 0
        clip: true
        // Light-blue line under Sort, edge to edge
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 12
            color: "#4FC3F7"
        }
        SortButton {
            id: sortButton
            anchors.left: parent.left
            anchors.leftMargin: 8
            onOrderChanged: page.reload()
        }
    }

    // Language filter: one chip per language that has favorites; tap to switch it on or off
    Flow {
        id: langBar
        anchors.top: sortBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: visible ? 4 : 0
        spacing: 8
        visible: page.languages.length > 1
        height: visible ? implicitHeight : 0
        Repeater {
            model: page.languages
            delegate: Button {
                required property var modelData
                objectName: "langChip_" + modelData.code
                checkable: true
                checked: page.shown[modelData.code] === true
                // the language's tint: strong when on, faint when off
                Material.background: Qt.alpha(page.langColor(modelData.code), checked ? 0.6 : 0.2)
                Material.elevation: 0
                text: modelData.name + " (" + modelData.count + ")"
                onClicked: page.toggleLanguage(modelData.code)
            }
        }
    }

    ListView {
        id: list
        anchors.top: langBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: page.cards
        ScrollBar.vertical: ScrollBar {}

        header: Item {
            width: list.width
            height: list.count > 0 ? hint.implicitHeight : 0
            HintLabel {
                id: hint
                width: parent.width
                leftPadding: 16
                rightPadding: 16
                topPadding: 8
                bottomPadding: 4
                visible: list.count > 0
                text: qsTr("Tap ★ on any card — in review, in a box, in the editor or in Lens — to add it here.")
            }
        }

        delegate: ItemDelegate {
            id: row
            required property var modelData
            width: ListView.view.width
            leftPadding: 12
            rightPadding: 4
            topPadding: 8
            bottomPadding: 8
            // Tinted like the language's chip
            background: Rectangle {
                color: Qt.alpha(page.langColor(row.modelData.language), row.down ? 0.28 : 0.14)
            }
            // The whole card: front, meaning and example, each with 🔊
            contentItem: CardFace {
                card: row.modelData
                note: (row.modelData.collection ? row.modelData.collection + " \u00B7 " : "") + page.boxName(row.modelData.box)
                StarButton {
                    implicitWidth: 36
                    cardId: row.modelData.id
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                height: 1
                color: Material.foreground
                opacity: 0.12
            }
            onClicked: page.StackView.view.push(editPage, { cardId: row.modelData.id })
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 320)
            visible: list.count === 0 && page.all.length === 0
            spacing: 8
            StarIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                color: Material.foreground
                opacity: 0.4
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: qsTr("No favorite words yet.\nTap ★ on any card — in review, in a box, in the editor or in Lens — to add it here.")
            }
        }
        // Favorites exist, but none in the switched-on languages
        Label {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 320)
            visible: list.count === 0 && page.all.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: qsTr("No favorite words in the selected languages.\nSwitch a language on above.")
        }
    }
}
