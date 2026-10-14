pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var control
    signal applyRequested(int percent)
    property bool dirty: false
    property var draftRevision: 0
    readonly property int displayedPercent: control !== undefined ? control.percent : -1
    readonly property var editRevision: control !== undefined ? control.revision : 0
    function syncDraft(force: bool) {
        if (force) dirty = false
        if (!dirty && !slider.pressed)
            slider.value = displayedPercent >= 0 ? displayedPercent : 0
    }
    function commit() {
        if (!dirty) { syncDraft(false); return }
        dirty = false
        if (draftRevision === editRevision && slider.enabled && Math.round(slider.value) !== displayedPercent)
            applyRequested(Math.round(slider.value))
        else
            syncDraft(false)
    }
    spacing: 0
    onDisplayedPercentChanged: syncDraft(false)
    onEditRevisionChanged: {
        dirty = false
        slider.value = displayedPercent >= 0 ? displayedPercent : 0
    }
    Component.onCompleted: syncDraft(true)
    onVisibleChanged: { if (!visible) commit() }
    RowLayout {
        Layout.fillWidth: true
        Label {
            objectName: "sidetoneReadback"
            Layout.fillWidth: true
            text: root.dirty ? qsTr("%1%").arg(Math.round(slider.value)) : root.control !== undefined ? root.control.display : ""
            font.pixelSize: Theme.smallSize
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Slider {
            id: slider
            objectName: "setting-sidetone"
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            from: 0
            to: 100
            stepSize: 1
            snapMode: Slider.SnapAlways
            focusPolicy: Qt.NoFocus
            enabled: root.control !== undefined && root.control.writable && root.control.percent >= 0
            Accessible.name: qsTr("Sidetone percentage")
            Accessible.description: root.control && root.control.reason ? root.control.reason : ""
            onMoved: {
                if (!pressed || root.draftRevision !== root.editRevision) return
                root.dirty = true
            }
            onPressedChanged: {
                if (pressed) root.draftRevision = root.editRevision
                else root.commit()
            }
        }
    }
}
