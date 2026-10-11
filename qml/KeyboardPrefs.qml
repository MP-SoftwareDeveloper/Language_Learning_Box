pragma Singleton
import QtCore
import QtQuick

// Keyboard options, stored in QSettings ("keyboard/*"); switched in Settings.
QtObject {
    // ä ö ü ß row under the text field while typing German (on by default: many phone keyboards lack ß)
    property alias germanKeys: store.germanKeys

    property Settings store: Settings {
        id: store
        category: "keyboard"
        property bool germanKeys: true
    }
}
