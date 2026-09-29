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
        contentHeight: 48

        RowLayout {
            anchors.fill: parent
            spacing: 0
            ToolButton {
                visible: stack.depth > 1 && stack.currentItem?.objectName !== "setupPage"
                text: "\u2190"
                font.pixelSize: 20
                onClicked: stack.pop()
            }
            Label {
                Layout.fillWidth: true
                text: stack.currentItem?.title ?? window.title
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                leftPadding: stack.depth > 1 && stack.currentItem?.objectName !== "setupPage" ? 0 : 16
            }
            // Help on every page (except Help itself)
            ToolButton {
                visible: stack.currentItem?.objectName !== "helpPage"
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    HelpIcon {
                        anchors.centerIn: parent
                        width: 24
                        height: 24
                        color: "white"
                    }
                }
                onClicked: stack.push(helpPage)
            }
            // Settings on every page (except Settings itself)
            ToolButton {
                visible: stack.currentItem?.objectName !== "settingsPage"
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    GearIcon {
                        anchors.centerIn: parent
                        width: 24
                        height: 24
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
        initialItem: HomePage {
            onReviewRequested: stack.push(reviewPage)
            onAddRequested: stack.push(editPage)
            onBrowseRequested: stack.push(listPage)
            onSendRequested: stack.push(sendPage)
            onDictionaryRequested: stack.push(dictionaryPage)
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
    Component { id: getPage; GetCardsPage {} }
    Component {
        id: setupPage
        SetupPage { onFinished: stack.pop(null) }
    }
    // First start: the setup (Simple or Full, starter words) before Home.
    Component.onCompleted: if (!AppMode.setupDone) stack.push(setupPage, {}, StackView.Immediate)
    Component { id: helpPage; HelpPage {} }

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
