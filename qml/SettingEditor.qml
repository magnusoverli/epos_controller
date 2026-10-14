pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var control
    required property string settingKey
    required property string label
    readonly property string errorText: control && control.error ? control.error : ""
    readonly property string explanation: errorText || (control && control.reason ? control.reason : "")
    signal valueRequested(int value)
    signal percentRequested(int percent)
    spacing: 2
    ComboBox {
        objectName: root.settingKey === "sidetone" ? "" : "setting-" + root.settingKey
        visible: root.settingKey !== "sidetone"
        enabled: root.control !== undefined && root.control.writable
        Layout.fillWidth: true
        model: root.control !== undefined ? root.control.options : []
        textRole: "label"
        valueRole: "value"
        currentIndex: root.control !== undefined ? root.control.currentIndex : -1
        displayText: root.control !== undefined ? root.control.display : ""
        Accessible.name: root.label
        Accessible.description: root.explanation
        onActivated: root.valueRequested(Number(currentValue))
    }
    Loader {
        active: root.settingKey === "sidetone"
        visible: active
        Layout.fillWidth: true
        sourceComponent: PercentSetting {
            control: root.control
            onApplyRequested: percent => root.percentRequested(percent)
        }
    }
    Label {
        objectName: "setting-error-" + root.settingKey
        visible: root.errorText.length > 0
        Layout.fillWidth: true
        text: root.errorText
        textFormat: Text.PlainText
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        font.pixelSize: Theme.smallSize
        color: Theme.error
    }
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && explanation.length > 0
    ToolTip.text: explanation
}
