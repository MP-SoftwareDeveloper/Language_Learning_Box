import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import LearningBox

ApplicationWindow {
    id: window
    width: 420
    height: 760
    visible: true
    title: qsTr("Learning Box")

    Material.theme: Material.System
    Material.accent: Material.Teal
    Material.primary: Material.Teal

    // Android 15 draws edge-to-edge: pad the toolbar below the status bar and
    // keep a fixed content height so it doesn't shrink when the back button hides.
    header: ToolBar {
        topPadding: SafeArea.margins.top
        leftPadding: SafeArea.margins.left
        rightPadding: SafeArea.margins.right
        contentHeight: 58   // 20% bigger than 48

        RowLayout {
            anchors.fill: parent
            spacing: 0
            ToolButton {
                visible: stack.depth > 1 && stack.currentItem?.objectName !== "setupPage"
                implicitWidth: 58
                implicitHeight: 58
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 38; height: 38
                        source: "../assets/back.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }
                onClicked: stack.pop()
            }
            Label {
                Layout.fillWidth: true
                text: stack.currentItem?.title ?? window.title
                font.pixelSize: 22
                font.bold: true
                elide: Text.ElideRight
                leftPadding: stack.depth > 1 && stack.currentItem?.objectName !== "setupPage" ? 0 : 16
            }
            // Help on every page (except Help itself)
            ToolButton {
                visible: stack.currentItem?.objectName !== "helpPage"
                contentItem: Item {
                    implicitWidth: 29
                    implicitHeight: 29
                    HelpIcon {
                        anchors.centerIn: parent
                        width: 29
                        height: 29
                        color: "white"
                    }
                }
                onClicked: stack.push(helpPage)
            }
            // Settings on every page (except Settings itself)
            ToolButton {
                visible: stack.currentItem?.objectName !== "settingsPage"
                contentItem: Item {
                    implicitWidth: 29
                    implicitHeight: 29
                    GearIcon {
                        anchors.centerIn: parent
                        width: 29
                        height: 29
                        color: "white"
                    }
                }
                onClicked: stack.push(settingsPage)
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        // Keep buttons clear of the gesture/navigation bar.
        anchors.bottomMargin: parent.SafeArea.margins.bottom
        // debug.bat self-test: open Lens by itself
        Component.onCompleted: if (AppMode.lensAutoTest()) push(lensPage)
        initialItem: HomePage {
            onReviewRequested: stack.push(reviewPage)
            onAddRequested: stack.push(editPage)
            onBrowseRequested: stack.push(listPage)
            onSendRequested: stack.push(sendPage)
            onDictionaryRequested: stack.push(dictionaryPage)
            onFavoritesRequested: stack.push(favoritesPage)
            onGetRequested: stack.push(getPage)
            onPacksRequested: stack.push(packsPage)
            onLensRequested: stack.push(lensPage)
            onBoxRequested: (box) => stack.push(boxPage, { box: box })
        }
    }

    Component { id: reviewPage; ReviewPage {} }
    Component { id: editPage; CardEditPage {} }
    Component { id: listPage; CardListPage {} }
    Component { id: packsPage; WordPacksPage {} }
    Component { id: lensPage; LensPage {} }
    Component { id: settingsPage; SettingsPage {} }
    Component { id: boxPage; BoxPage {} }
    Component { id: sendPage; SendCardsPage {} }
    Component { id: dictionaryPage; DictionaryPage {} }
    Component { id: favoritesPage; FavoritesPage {} }
    Component { id: getPage; GetCardsPage {} }
    Component {
        id: setupPage
        SetupPage { onFinished: stack.pop(null) }
    }
    // First start: the setup (Simple or Full, starter words) before Home.
    // First run: the setup wizard finishes after start-up, so create the A1/A2 level boxes then
    Connections {
        target: AppMode
        function onChanged() { if (AppMode.setupDone) LevelPacks.installBoxes(false) }
    }

    Component.onCompleted: {
        if (Backup.restored) AppMode.setupDone = true // cards came back from the backup: no setup wizard
        if (AppMode.setupDone) LevelPacks.installBoxes(false) // A1/A2 levels as their own learning boxes (first start only)
        if (!AppMode.setupDone) stack.push(setupPage, {}, StackView.Immediate)
    }
    Component { id: helpPage; HelpPage {} }

    // ---- Keep the cards safe: one-time permission for the automatic backup (Android) ----
    Settings {
        id: backupPrefs
        category: "backup"
        property double askAgainAfter: 0   // ms since epoch; "Later" waits two days
    }
    Timer {
        id: backupAsk
        interval: 2000
        running: Qt.platform.os === "android" && !Backup.accessGranted && Date.now() > backupPrefs.askAgainAfter
        onTriggered: backupDialog.open()
    }
    // A new installation with a copy of the old cards in Documents: offer to bring them back
    Connections {
        target: Backup
        function onRestoreAvailableChanged() { if (Backup.restoreAvailable) restoreDialog.open() }
    }
    Dialog {
        id: restoreDialog
        property bool done: false
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        modal: true
        closePolicy: Popup.NoAutoClose
        title: done ? qsTr("Cards restored") : qsTr("Your cards were found")
        standardButtons: Dialog.NoButton
        Component.onCompleted: if (Backup.restoreAvailable) open()
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: restoreDialog.done
                      ? qsTr("Your cards are back. The app closes now \u2013 open it again.")
                      : qsTr("A backup with %n card(s) from an earlier installation was found on this phone. Bring your cards back?", "", Backup.copyCards)
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    visible: !restoreDialog.done
                    text: qsTr("Start without them")
                    onClicked: { Backup.keepCurrent(); restoreDialog.close() }
                }
                Button {
                    highlighted: true
                    text: restoreDialog.done ? qsTr("Close app") : qsTr("Restore my cards")
                    onClicked: {
                        if (restoreDialog.done) {
                            Qt.quit()
                        } else if (Backup.restoreNow()) {
                            restoreDialog.done = true
                        } else {
                            restoreDialog.close()
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: backupDialog
        anchors.centerIn: parent
        width: Math.min(window.width - 32, 400)
        modal: true
        title: qsTr("Keep your cards safe")
        standardButtons: Dialog.NoButton
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Allow Learning Box to keep a copy of your cards in your phone's Documents folder. "
                           + "Then an update or a new installation of the app never loses your cards: they come back by themselves.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: qsTr("On the next screen, switch on \u201CAllow access to manage all files\u201D and come back.")
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    text: qsTr("Later")
                    onClicked: {
                        backupPrefs.askAgainAfter = Date.now() + 2 * 24 * 3600 * 1000
                        backupDialog.close()
                    }
                }
                Button {
                    highlighted: true
                    text: qsTr("Allow")
                    onClicked: {
                        backupDialog.close()
                        Backup.requestAccess()
                    }
                }
            }
        }
    }

    // Due counts depend on the date: refresh when the app comes back to the foreground.
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive)
                CardStore.refresh()
        }
    }

    // Android back key closes the window; pop a page instead while we can.
    onClosing: (close) => {
        if (stack.depth > 1 && stack.currentItem?.objectName !== "setupPage") {
            close.accepted = false
            stack.pop()
        }
    }
}
