pragma Singleton
import QtQuick

// One place for the names of the six boxes (1..5 = Leitner boxes, 6 = learned).
QtObject {
    readonly property var names: [qsTr("New"), qsTr("Started"), qsTr("Familiar"),
                                  qsTr("Practised"), qsTr("Strong"), qsTr("Mastered")]
    // "New (reg1)" ... "Mastered (reg6)"
    function name(box) {
        const b = Math.max(1, Math.min(6, box))
        return names[b - 1] + " (reg" + b + ")"
    }
    // all six, for ComboBox models
    readonly property var all: [name(1), name(2), name(3), name(4), name(5), name(6)]
    // compact label for narrow tabs
    function code(box) { return "reg" + Math.max(1, Math.min(6, box)) }
}
