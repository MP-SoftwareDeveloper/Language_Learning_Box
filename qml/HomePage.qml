import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

Page {
    id: page
    title: qsTr("Learning Box")

    signal reviewRequested()
    signal addRequested()
    signal browseRequested()
    signal dictionaryRequested()
    signal sendRequested()
    signal getRequested()
    signal packsRequested()
    signal lensRequested()
    signal favoritesRequested()
    signal boxRequested(int box) // 1..5, 6 = Learned

    // Every learning box has its own colour (by its id); the chooser, the list and the six box
    // tiles use it. The tiles fade from a light to a deep shade of it (Box 1 -> Learned).
    readonly property var boxPalette: ["#1E88E5", "#43A047", "#8E24AA", "#F4511E", "#00897B", "#E53935",
                                       "#3949AB", "#6D4C41", "#D81B60", "#FB8C00", "#546E7A", "#7CB342"]
    function boxColor(id) { return boxPalette[(Math.max(1, id) - 1) % boxPalette.length] }
    function boxLight(id) { return Qt.tint(boxColor(id), Qt.rgba(1, 1, 1, 0.82)) } // pale background
    readonly property color boxBase: boxColor(CardStore.currentCollection)
    readonly property color comboColor: boxLight(CardStore.currentCollection)
    function tileColor(i) {
        const light = Qt.tint(boxBase, Qt.rgba(1, 1, 1, 0.38))
        const dark = Qt.darker(boxBase, 1.25)
        return Qt.tint(light, Qt.rgba(dark.r, dark.g, dark.b, i / 5))
    }
    readonly property color selectedText: "#0D2A45"
    // Flag of the language learned in a learning box
    function flag(lang) { return lang === "en" ? "\uD83C\uDDFA\uD83C\uDDF8" : "\uD83C\uDDE9\uD83C\uDDEA" }
    readonly property bool english: CardStore.learningLanguage === "en"
    property int menuId: 0
    property string menuName: ""

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 16

            Item { Layout.preferredHeight: 4 }

            // ---- Learning box chooser ----
            // A ComboBox whose closed state already shows the first two learning boxes
            // (the selected one, highlighted, and the one used before it — tap to switch).
            // The arrow opens the full list. The list is ordered by last use, so the
            // selected box is always first and stays first after a restart.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 4
                spacing: 0

                ComboBox {
                    id: boxCombo
                    Layout.fillWidth: true
                    model: CardStore.collections
                    textRole: "name"
                    currentIndex: 0
                    Material.foreground: page.selectedText // arrow, readable on the light background
                    background: Rectangle {
                        implicitHeight: 48
                        radius: 6
                        color: boxCombo.pressed ? Qt.darker(page.comboColor, 1.05) : page.comboColor
                    }
                    onActivated: (index) => {
                        const id = CardStore.collections[index].id
                        CardStore.selectCollection(id)
                        currentIndex = 0
                    }

                    contentItem: Column {
                        leftPadding: 4
                        topPadding: 6
                        bottomPadding: 6
                        spacing: 2
                        // Selected learning box
                        Label {
                            width: Math.min(implicitWidth, boxCombo.availableWidth - 8)
                            text: page.flag(CardStore.learningLanguage) + "  " + CardStore.currentCollectionName
                            elide: Text.ElideRight
                            font.pixelSize: 17
                            font.bold: true
                            color: page.selectedText
                            leftPadding: 6
                            rightPadding: 6
                            topPadding: 2
                            bottomPadding: 2
                        }
                        // Second option, shown without opening the list.
                        Label {
                            width: boxCombo.availableWidth - 8
                            visible: CardStore.collections.length > 1
                            text: CardStore.collections.length > 1
                                  ? page.flag(CardStore.collections[1].language) + "  " + CardStore.collections[1].name
                                    + "  ·  " + qsTr("%n due", "", CardStore.collections[1].due)
                                  : ""
                            elide: Text.ElideRight
                            leftPadding: 6
                            color: page.selectedText
                            opacity: secondTap.pressed ? 0.4 : 0.75
                            MouseArea {
                                id: secondTap
                                anchors.fill: parent
                                onClicked: {
                                    CardStore.selectCollection(CardStore.collections[1].id)
                                    boxCombo.currentIndex = 0
                                }
                            }
                        }
                    }

                    delegate: ItemDelegate {
                        required property var modelData
                        required property int index
                        width: ListView.view ? ListView.view.width : implicitWidth
                        // The learning box's own colour: pale background, strong stripe on the left
                        background: Rectangle {
                            color: page.boxLight(modelData.id)
                            opacity: parent.pressed ? 0.8 : 1
                            Rectangle {
                                width: 6
                                height: parent.height
                                color: page.boxColor(modelData.id)
                            }
                        }
                        contentItem: ColumnLayout {
                            spacing: 0
                            // Full name (wraps instead of being cut off)
                            Label {
                                Layout.fillWidth: true
                                text: page.flag(modelData.language) + "  " + modelData.name
                                wrapMode: Text.WordWrap
                                font.bold: modelData.current
                                color: page.selectedText
                                leftPadding: 12
                                rightPadding: 6
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("%1 cards").arg(modelData.total) + "  \u00B7  " + qsTr("%n due", "", modelData.due)
                                opacity: 0.7
                                color: page.selectedText
                                font.pixelSize: 13
                                leftPadding: 12
                            }
                        }
                    }
                    // The list is as wide as the page, not only as wide as the combo box
                    popup.width: page.width - 16
                }
                ToolButton {
                    text: "+"
                    font.pixelSize: 22
                    onClicked: newDialog.open()
                }
                ToolButton {
                    text: "⋮"
                    font.pixelSize: 20
                    onClicked: {
                        page.menuId = CardStore.currentCollection
                        page.menuName = CardStore.currentCollectionName
                        boxMenu.popup()
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                columns: 3
                rowSpacing: 8
                columnSpacing: 8

                Repeater {
                    model: 6
                    // Tap a box to see its cards and move them between boxes.
                    delegate: Pane {
                        required property int index
                        Layout.fillWidth: true
                        id: tile
                        readonly property color base: page.tileColor(index)
                        padding: 10
                        // Fades from the learning box's light shade (Box 1) to its deep shade (Learned).
                        background: Rectangle {
                            radius: 8
                            opacity: boxTap.pressed ? 0.8 : 1
                            gradient: Gradient {
                                GradientStop { position: 0; color: Qt.lighter(tile.base, 1.3) }
                                GradientStop { position: 1; color: tile.base }
                            }
                        }
                        TapHandler {
                            id: boxTap
                            onTapped: page.boxRequested(index + 1)
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            Label {
                                text: index < 5 ? qsTr("Box %1").arg(index + 1) : qsTr("Learned")
                                color: "white"
                                opacity: 0.85
                            }
                            Label {
                                text: index < 5 ? (CardStore.boxCounts[index] ?? 0) : CardStore.learnedCount
                                color: "white"
                                font.pixelSize: 26
                                font.bold: true
                            }
                        }
                    }
                }
            }

            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: CardStore.totalCount === 0
                      ? qsTr("This learning box is empty. Add words from the Dictionary below, or tap Box 1 and then +.")
                      : qsTr("%n card(s) due today", "", CardStore.dueCount)
                font.pixelSize: 16
            }

            // Start — double height, rainbow tail on rounded edge
            Item {
                id: startItem
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                property real tailAngle: 0
                NumberAnimation on tailAngle {
                    from: 0; to: 360; duration: 2000
                    loops: Animation.Infinite; running: true
                }

                // Button with explicit radius 8 background (matching box tiles)
                Button {
                    id: startBtn
                    anchors.fill: parent
                    highlighted: true
                    enabled: CardStore.dueCount > 0
                    onClicked: page.reviewRequested()

                    background: Rectangle {
                        radius: 8
                        color: Material.accentColor
                        opacity: startBtn.enabled ? 1.0 : 0.38
                    }

                    // "Start" — 3× font, single color that cycles every 800ms
                    property var rainbowColors: ["#ff5555", "#ffaa00", "#ffe000", "#44ff88", "#33d6ff", "#5577ff", "#cc44ff"]
                    property int colorIdx: 0
                    property color startTextColor: rainbowColors[colorIdx]

                    Timer {
                        interval: 800
                        running: startBtn.enabled
                        repeat: true
                        onTriggered: startBtn.colorIdx = (startBtn.colorIdx + 1) % startBtn.rainbowColors.length
                    }

                    contentItem: Label {
                        text: "Start"
                        font.pixelSize: 42
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: startBtn.enabled ? startBtn.startTextColor : "#80ffffff"
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }

                // Rainbow tail follows the rounded-rect edge — point-based (no setLineDash, no glitches)
                Canvas {
                    anchors.fill: parent
                    property real angle: startItem.tailAngle
                    onAngleChanged: requestPaint()

                    function perimPt(pos, w, h, lw, br) {
                        var half = lw / 2
                        var L1 = w - lw - 2 * br   // top/bottom straight length
                        var L2 = h - lw - 2 * br   // left/right straight length
                        var arcLen = (Math.PI / 2) * br
                        var P = 2 * L1 + 2 * L2 + 4 * arcLen
                        pos = ((pos % P) + P) % P

                        if (pos < L1) return { x: half + br + pos, y: half }
                        pos -= L1
                        if (pos < arcLen) {
                            var a1 = -Math.PI/2 + (pos/arcLen) * (Math.PI/2)
                            return { x: (w-half-br) + br*Math.cos(a1), y: (half+br) + br*Math.sin(a1) }
                        }
                        pos -= arcLen
                        if (pos < L2) return { x: w - half, y: half + br + pos }
                        pos -= L2
                        if (pos < arcLen) {
                            var a2 = (pos/arcLen) * (Math.PI/2)
                            return { x: (w-half-br) + br*Math.cos(a2), y: (h-half-br) + br*Math.sin(a2) }
                        }
                        pos -= arcLen
                        if (pos < L1) return { x: (w-half-br) - pos, y: h - half }
                        pos -= L1
                        if (pos < arcLen) {
                            var a3 = Math.PI/2 + (pos/arcLen) * (Math.PI/2)
                            return { x: (half+br) + br*Math.cos(a3), y: (h-half-br) + br*Math.sin(a3) }
                        }
                        pos -= arcLen
                        if (pos < L2) return { x: half, y: (h-half-br) - pos }
                        pos -= L2
                        var a4 = Math.PI + (pos/arcLen) * (Math.PI/2)
                        return { x: (half+br) + br*Math.cos(a4), y: (half+br) + br*Math.sin(a4) }
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var w = width, h = height, lw = 5, br = 8
                        var half = lw / 2
                        var L1 = w - lw - 2*br, L2 = h - lw - 2*br
                        var arcLen = (Math.PI/2) * br
                        var P = 2*L1 + 2*L2 + 4*arcLen
                        var tailLen = P * 0.4
                        var head = (startItem.tailAngle / 360) * P
                        var pal = [[255,0,0],[255,128,0],[255,255,0],[0,255,0],[0,200,255],[0,0,255],[200,0,255]]
                        var N = 30
                        for (var i = 0; i < N; i++) {
                            var t0 = i / N, t1 = (i + 1) / N
                            var from = head - (1 - t0) * tailLen
                            var to   = head - (1 - t1) * tailLen
                            var ci = t0 * (pal.length - 1)
                            var i0 = Math.floor(ci), i1 = Math.min(i0 + 1, pal.length - 1), f = ci - i0
                            var c0 = pal[i0], c1 = pal[i1]
                            var R = Math.round(c0[0] + f*(c1[0]-c0[0]))
                            var G = Math.round(c0[1] + f*(c1[1]-c0[1]))
                            var B = Math.round(c0[2] + f*(c1[2]-c0[2]))
                            ctx.strokeStyle = "rgba("+R+","+G+","+B+","+(0.2+0.8*t0)+")"
                            ctx.lineWidth = lw
                            ctx.lineCap = (i === N - 1) ? "round" : "butt"
                            var p0 = perimPt(from, w, h, lw, br)
                            var p1 = perimPt(to, w, h, lw, br)
                            ctx.beginPath()
                            ctx.moveTo(p0.x, p0.y)
                            ctx.lineTo(p1.x, p1.y)
                            ctx.stroke()
                        }
                    }
                }
            }
            Button {
                id: lensBtn
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                visible: AppMode.full
                Layout.preferredHeight: 67   // 70% of Start (96)
                text: qsTr("Lens \u00B7 words from a photo")
                contentItem: Item {
                    implicitHeight: lensRow.implicitHeight
                    implicitWidth: lensRow.implicitWidth
                    Row {
                        id: lensRow
                        anchors.centerIn: parent
                        spacing: 10
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 44; height: 44
                            source: "../assets/lens.png"
                            fillMode: Image.PreserveAspectFit
                            mipmap: true
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: lensBtn.text
                            font: lensBtn.font
                            color: lensBtn.Material.foreground
                        }
                    }
                }
                onClicked: page.lensRequested()
                Component.onCompleted: if (background) background.radius = 4
            }
            // Starred cards (★ on any card)
            Button {
                id: favButton
                objectName: "favoritesButton"
                Material.background: "#E65100"
                Material.foreground: "white"
                Layout.fillWidth: true
                Layout.preferredHeight: 67   // 70% of Start (96)
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                text: qsTr("Favorite words (%1)").arg(CardStore.favoriteCount)
                contentItem: Item {
                    implicitHeight: favRow.implicitHeight
                    implicitWidth: favRow.implicitWidth
                    Row {
                        id: favRow
                        anchors.centerIn: parent
                        spacing: 8
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 44; height: 44
                            source: "../assets/favorites.png"
                            fillMode: Image.PreserveAspectFit
                            mipmap: true
                            opacity: CardStore.favoriteCount > 0 ? 1 : 0.7
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: favButton.text
                            font: favButton.font
                            color: favButton.Material.foreground
                        }
                    }
                }
                onClicked: page.favoritesRequested()
                Component.onCompleted: if (background) background.radius = 4
            }
            // Dictionary (right after Favorite words)
            Button {
                id: dictButton
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 67   // 70% of Start (96)
                text: qsTr("Dictionary")
                Material.background: "#7B1FA2"
                Material.foreground: "white"
                contentItem: Item {
                    implicitHeight: dictRow.implicitHeight
                    implicitWidth: dictRow.implicitWidth
                    Row {
                        id: dictRow
                        anchors.centerIn: parent
                        spacing: 10
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 44; height: 44
                            source: "../assets/dictionary.png"
                            fillMode: Image.PreserveAspectFit
                            mipmap: true
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: dictButton.text
                            font: dictButton.font
                            color: dictButton.Material.foreground
                        }
                    }
                }
                onClicked: page.dictionaryRequested()
                Component.onCompleted: if (background) background.radius = 4
            }

            // Export and import side by side (each page has a ? with a short how-to)
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 4
                spacing: 12

                Button {
                    id: exportButton
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 67   // 70% of Start (96)
                    enabled: CardStore.totalCount > 0
                    text: qsTr("Export")
                    Material.background: "#1976D2"
                    Material.foreground: "white"
                    contentItem: Item {
                        implicitHeight: exportRow.implicitHeight
                        implicitWidth: exportRow.implicitWidth
                        Row {
                            id: exportRow
                            anchors.centerIn: parent
                            spacing: 8
                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 34; height: 34
                                source: "../assets/export.png"
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                                opacity: exportButton.enabled ? 1 : 0.4
                            }
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: exportButton.text
                                font: exportButton.font
                                color: exportButton.Material.foreground
                            }
                        }
                    }
                    onClicked: page.sendRequested()
                    Component.onCompleted: if (background) background.radius = 4
                }
                Button {
                    id: importButton
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 67   // 70% of Start (96)
                    text: qsTr("Import")
                    Material.background: "#2E7D32"
                    Material.foreground: "white"
                    contentItem: Item {
                        implicitHeight: importRow.implicitHeight
                        implicitWidth: importRow.implicitWidth
                        Row {
                            id: importRow
                            anchors.centerIn: parent
                            spacing: 8
                            Image {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 34; height: 34
                                source: "../assets/import.png"
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                            }
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: importButton.text
                                font: importButton.font
                                color: importButton.Material.foreground
                            }
                        }
                    }
                    onClicked: page.getRequested()
                    Component.onCompleted: if (background) background.radius = 4
                }
            }

            // Simple mode: what Full mode would add
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                visible: !AppMode.full
                padding: 12
                background: Rectangle {
                    radius: 8
                    color: "transparent"
                    border.width: 1
                    border.color: Material.accentColor
                }
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: qsTr("More features")
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Lens (words from photos), meanings and example sentences filled in for you, word packs, pictures on cards …")
                    }
                    Button {
                        flat: true
                        text: qsTr("See what the Full app adds")
                        onClicked: compareDialog.open()
                    }
                }
            }

            // Only when the voice for this learning box is missing (speed: on the review card / Settings)
            Label {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: Speaker.ready && !Speaker.voiceAvailable
                color: Material.color(Material.Orange)
                text: qsTr("No %1 voice is installed. On Android: Settings → Text-to-speech → install the %1 voice data.").arg(Speaker.voiceName)
            }
            Button {
                Layout.leftMargin: 16
                flat: true
                visible: Speaker.ready && !Speaker.voiceAvailable && Qt.platform.os === "android"
                text: qsTr("Get the %1 voice (Speech Recognition and Synthesis from Google)").arg(Speaker.voiceName)
                onClicked: Qt.openUrlExternally("https://play.google.com/store/apps/details?id=com.google.android.tts")
            }

            // All cards: last button on Home
            Button {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.preferredHeight: 67   // 70% of Start (96)
                enabled: CardStore.totalCount > 0
                text: qsTr("All cards (%1)").arg(CardStore.totalCount)
                Material.background: "#455A64"
                Material.foreground: "white"
                onClicked: page.browseRequested()
                Component.onCompleted: if (background) background.radius = 4
            }

            Label {
                Layout.leftMargin: 16
                visible: CardStore.lastError.length > 0
                color: Material.color(Material.Red)
                text: CardStore.lastError
            }
            Item { Layout.preferredHeight: 16 }
        }
    }
    Menu {
        id: boxMenu
        MenuItem {
            text: qsTr("Rename")
            onTriggered: {
                renameField.text = page.menuName
                renameDialog.open()
            }
        }
        MenuItem {
            text: qsTr("Word packs · %1").arg(WordPacks.title)
            // German words: Full app, German learning boxes
            enabled: AppMode.full && !page.english
            height: enabled ? implicitHeight : 0
            visible: enabled
            onTriggered: page.packsRequested()
        }
        MenuItem {
            text: qsTr("Start over (all cards to Box 1)")
            enabled: CardStore.totalCount > 0
            onTriggered: resetDialog.open()
        }
        MenuItem {
            text: qsTr("Delete")
            enabled: CardStore.collections.length > 1
            onTriggered: deleteDialog.open()
        }
    }

    ResetDialog { id: resetDialog }

    Dialog {
        id: compareDialog
        title: qsTr("Simple and Full app")
        modal: true
        width: Math.min(360, page.width - 16)
        height: Math.min(implicitHeight, page.height - 16)
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 2)
        contentItem: Flickable {
            implicitHeight: compareColumn.implicitHeight
            contentHeight: compareColumn.implicitHeight
            clip: true
            ColumnLayout {
                id: compareColumn
                width: parent.width
                ModeComparison { Layout.fillWidth: true }
                HintLabel {
                    Layout.topMargin: 8
                    text: qsTr("Your cards and progress are kept when you switch. You can switch back in Settings.")
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                flat: true
                text: qsTr("Not now")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                flat: true
                text: qsTr("Switch to Full")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: AppMode.mode = "full"
    }

    Dialog {
        id: newDialog
        title: qsTr("New learning box")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string meaning: "fa"
        onAboutToShow: {
            newName.text = ""
            meaning = "fa"
            learnGerman.checked = true
            startWith.currentIndex = 0
            newName.forceActiveFocus()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            TextField {
                id: newName
                Layout.fillWidth: true
                placeholderText: learnEnglish.checked ? qsTr("e.g. English A2") : qsTr("e.g. Netzwerk neu A2")
            }
            Label { text: qsTr("I want to learn") }
            RowLayout {
                ButtonGroup { id: learnGroup }
                RadioButton {
                    id: learnGerman
                    ButtonGroup.group: learnGroup
                    checked: true
                    text: "\uD83C\uDDE9\uD83C\uDDEA Deutsch"
                }
                RadioButton {
                    id: learnEnglish
                    ButtonGroup.group: learnGroup
                    text: "\uD83C\uDDFA\uD83C\uDDF8 English"
                    onCheckedChanged: {
                        startWith.currentIndex = 0
                        if (newDialog.meaning === (checked ? "en" : "de"))
                            newDialog.meaning = "fa" // never the language being learned
                    }
                }
            }
            HintLabel {
                visible: learnEnglish.checked
                text: qsTr("Read aloud with an American voice.")
            }
            Label { text: qsTr("Meanings in") }
            RowLayout {
                Repeater {
                    // Every language except the one being learned
                    model: [ { code: "fa", label: "فارسی" }, { code: "en", label: "English" }, { code: "de", label: "Deutsch" } ]
                        .filter(o => o.code !== (learnEnglish.checked ? "en" : "de"))
                    delegate: RadioButton {
                        required property var modelData
                        checkable: false // shows newDialog.meaning, never its own state
                        text: modelData.label
                        checked: newDialog.meaning === modelData.code
                        onClicked: newDialog.meaning = modelData.code
                    }
                }
            }
            Label { text: qsTr("Start with"); visible: !learnEnglish.checked }
            ComboBox {
                id: startWith
                Layout.fillWidth: true
                visible: !learnEnglish.checked // starter words and word packs are German
                // Word pack only in the Full app
                model: AppMode.full ? [qsTr("Empty box"), qsTr("%1 starter words (German, English, Persian)").arg(WordPacks.starterTotal), qsTr("Word pack: %1").arg(WordPacks.title)]
                                    : [qsTr("Empty box"), qsTr("%1 starter words (German, English, Persian)").arg(WordPacks.starterTotal)]
            }
        }
        onAccepted: {
            const name = newName.text.trim()
            const id = CardStore.createCollection(name.length > 0 ? name : qsTr("Learning box %1").arg(CardStore.collections.length + 1),
                                                  learnEnglish.checked ? "en" : "de", newDialog.meaning)
            if (id > 0) {
                CardStore.selectCollection(id)
                if (startWith.currentIndex === 1)
                    WordPacks.addStarterCards()
                else if (startWith.currentIndex === 2)
                    WordPacks.addAll()
            }
        }
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename learning box")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: renameField
            anchors.left: parent.left
            anchors.right: parent.right
        }
        onAccepted: {
            if (renameField.text.trim().length > 0)
                CardStore.renameCollection(page.menuId, renameField.text.trim())
        }
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete learning box?")
        modal: true
        width: 320
        x: (page.width - width) / 2
        y: Math.max(8, (page.height - height) / 3)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            wrapMode: Text.WordWrap
            text: qsTr("\"%1\" and all its cards will be deleted. This cannot be undone.").arg(page.menuName)
        }
        onAccepted: CardStore.deleteCollection(page.menuId)
    }
}
