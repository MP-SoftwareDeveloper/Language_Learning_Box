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
        active: page.visible && cameraPermission.status === Qt.PermissionStatus.Granted
        sourceComponent: Item {
            readonly property alias capture: imageCapture
            CaptureSession {
                camera: Camera {
                    active: true
                    onErrorOccurred: (error, message) => status.text = message
                }
                imageCapture: ImageCapture {
                    id: imageCapture
                    onImageSaved: (id, path) => {
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
                        page.busy = false
                        status.text = message
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

    RoundButton {
        id: shutter
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 24 }
        width: 76; height: 76
        readonly property var capture: cameraLoader.item ? cameraLoader.item.capture : null
        enabled: capture !== null && capture.readyForCapture && !page.busy
        Material.background: "white"
        contentItem: Rectangle { radius: width / 2; color: shutter.enabled ? Material.accent : "#9e9e9e"; anchors.margins: 6 }
        onClicked: {
            status.text = ""
            page.busy = true
            page.shotRotation = tilt.available ? tilt.rotation : 0
            capture.captureToFile(CardStore.cameraFilePath())
        }
    }
}
