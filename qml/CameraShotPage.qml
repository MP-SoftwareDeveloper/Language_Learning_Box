import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtMultimedia
import LearningBox

// Take a picture for a card with the device camera. Emits taken(name) with the stored
// picture (see CardStore.importCameraShot) and closes itself.
Page {
    id: page
    title: qsTr("Take photo")

    signal taken(string name)

    property int shotRotation: 0
    property bool busy: false
    // The camera did not become ready even after restarts: the shutter then takes the viewfinder picture
    property bool stuck: false

    // On some Android devices ImageCapture occasionally never reaches "ready" after the
    // camera is created, leaving the shutter stuck disabled. Toggling this briefly true
    // forces the Loader below to destroy and recreate the whole camera session.
    property bool cameraKick: false
    property int kickCount: 0           // restarts since the viewfinder was shown
    readonly property int maxKicks: 2
    // Recreate the camera session. Android closes a camera asynchronously: opening it again in the same moment
    // often fails (black preview, shutter stays grey), so it is left closed for a while first (kickGap).
    function restartCamera() {
        cameraKick = true
        kickGap.restart()
    }

    CameraPermission { id: cameraPermission }
    Component.onCompleted: {
        if (cameraPermission.status !== Qt.PermissionStatus.Granted)
            cameraPermission.request()
    }

    // Landscape shots with the rotation lock on are turned upright, as in Lens.
    DeviceTilt {
        id: tilt
        active: cameraLoader.active
    }

    Rectangle { anchors.fill: parent; color: "black" }

    Loader {
        id: cameraLoader
        anchors.fill: parent
        active: page.visible && cameraPermission.status === Qt.PermissionStatus.Granted && !page.cameraKick
        onLoaded: readyWatchdog.restart()
        onActiveChanged: if (!active && !page.cameraKick) page.kickCount = 0 // shown again later: start fresh
        sourceComponent: Item {
            readonly property alias capture: imageCapture
            readonly property var sink: viewfinder.videoSink
            CaptureSession {
                camera: Camera {
                    active: true
                    onErrorOccurred: (error, message) => status.text = message
                }
                imageCapture: ImageCapture {
                    id: imageCapture
                    onReadyForCaptureChanged: if (readyForCapture) { status.text = ""; page.stuck = false } // "restarting it…" is over
                    onImageSaved: (id, path) => {
                        shotWatchdog.stop()
                        const name = CardStore.importCameraShot(path, page.shotRotation)
                        page.busy = false
                        if (name === "") {
                            status.text = qsTr("The photo could not be saved.")
                            return
                        }
                        page.taken(name)
                        page.StackView.view.pop()
                    }
                    onErrorOccurred: (id, error, message) => {
                        shotWatchdog.stop()
                        if (page.busy && page.useViewfinderCopy())
                            return // the still capture failed, the viewfinder picture was taken instead
                        page.busy = false
                        status.text = message
                        page.recoverCamera() // a failed shot often leaves the camera stuck: start it again
                    }
                }
                videoOutput: viewfinder
            }
            VideoOutput {
                id: viewfinder
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
            }
        }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 48
        visible: cameraPermission.status !== Qt.PermissionStatus.Granted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: "white"
        text: qsTr("Camera access is needed to take a photo.")
    }

    Label {
        id: status
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 12 }
        visible: text.length > 0
        padding: 8
        color: "white"
        background: Rectangle { color: "#99000000"; radius: 6 }
    }

    // A stuck camera is restarted by itself, so the user does not have to leave this page and come back.
    function recoverCamera() {
        if (page.kickCount >= page.maxKicks) {
            // Restarting does not help on this phone: the shutter now takes the viewfinder picture instead
            page.stuck = true
            status.text = qsTr("The camera is slow. You can still take the photo with the button.")
            return
        }
        page.kickCount++
        status.text = qsTr("Camera is taking a moment — restarting it…")
        page.restartCamera()
    }

    // Store the viewfinder copy kept when the shutter was pressed as the picture (see CardStore.grabFrame),
    // used when the still capture hangs or fails. Returns false when there is no usable copy.
    function useViewfinderCopy() {
        const name = CardStore.importGrabbedShot(page.shotRotation)
        if (name === "")
            return false
        page.busy = false
        page.taken(name)
        page.StackView.view.pop()
        return true
    }

    // Self-healing: if the shutter is still disabled a few seconds after the camera session was created,
    // restart the session instead of leaving it stuck. Later tries wait a little longer; after maxKicks
    // restarts it gives up and says so.
    Timer {
        id: readyWatchdog
        interval: 4000 + page.kickCount * 4000
        repeat: false
        onTriggered: {
            if (page.busy)
                return // the shot watchdog handles a picture that is being taken
            const cap = cameraLoader.item ? cameraLoader.item.capture : null
            if (cap && !cap.readyForCapture)
                page.recoverCamera()
        }
    }
    // Picture taken but never finished (the shutter stays disabled, seen on some phones when Android closes the
    // camera right after the shutter was pressed): start the camera again.
    Timer {
        id: shotWatchdog
        interval: 4000
        repeat: false
        onTriggered: {
            if (!page.busy)
                return
            if (page.useViewfinderCopy())
                return // no need to restart the camera: the picture is taken
            page.busy = false
            page.recoverCamera()
        }
    }
    // Leave the camera closed for a moment before creating it again (see restartCamera)
    Timer {
        id: kickGap
        interval: 1500
        repeat: false
        onTriggered: page.cameraKick = false
    }

    RoundButton {
        id: shutter
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 24 }
        width: 76; height: 76
        readonly property var capture: cameraLoader.item ? cameraLoader.item.capture : null
        enabled: ((capture !== null && capture.readyForCapture) || page.stuck) && !page.busy
        Material.background: "white"
        contentItem: Rectangle { radius: width / 2; color: shutter.enabled ? Material.accent : "#9e9e9e"; anchors.margins: 6 }
        onClicked: {
            status.text = ""
            page.shotRotation = tilt.available ? tilt.rotation : 0
            // Keep a copy of the viewfinder picture: used if the still capture hangs
            const copied = cameraLoader.item ? CardStore.grabFrame(cameraLoader.item.sink) : false
            if (capture === null || !capture.readyForCapture) {
                // stuck camera: the viewfinder picture is the photo
                if (!copied || !page.useViewfinderCopy())
                    status.text = qsTr("The camera is not ready. Go back and open Take photo again.")
                return
            }
            page.busy = true
            shotWatchdog.restart()
            capture.captureToFile(CardStore.cameraFilePath())
        }
    }
}
