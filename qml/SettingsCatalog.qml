pragma Singleton
import QtQuick

QtObject {
    // Stable keys identify future capabilities; labels are presentation only.
    // This is an inventory, not defaults or device readback.
    readonly property var device: [
        { sectionKey: "audio", section: qsTr("CALL AND AUDIO") },
        { key: "ancMode", label: qsTr("ANC and Talkthrough") },
        { key: "adaptiveAnc", label: qsTr("Adaptive") },
        { key: "ancLevel", label: qsTr("ANC level") },
        { key: "sidetone", label: qsTr("Sidetone") },
        { key: "audioLimiter", label: qsTr("Audio Limiter") },
        { key: "answerOnBoomDown", label: qsTr("Answer Call — By moving boom arm down") },
        { key: "answerOnWear", label: qsTr("Answer Call — By putting headset on") },
        { key: "muteOnBoomUp", label: qsTr("Mute Call — By moving boom arm up") },
        { key: "muteOnRemoval", label: qsTr("Mute Call — By removing headset") },
        { key: "boomSide", label: qsTr("Boom Arm Side") },
        { key: "autoPause", label: qsTr("Auto Pause") },
        { sectionKey: "sounds", section: qsTr("SOUNDS & NOTIFICATIONS") },
        { key: "prompts", label: qsTr("Tone & Voice Prompts") },
        { key: "promptLanguage", label: qsTr("Voice Prompt Language") },
        { key: "noPromptsDuringCalls", label: qsTr("No Prompts During Calls") },
        { key: "productRingtone", label: qsTr("Product Ringtone") },
        { key: "busylight", label: qsTr("Busylight") },
        { key: "muteNotification", label: qsTr("Mute Notification") },
        { key: "muteInterval", label: qsTr("Mute Interval") },
        { key: "talkWhileMuted", label: qsTr("Talk While Muted") },
        { sectionKey: "device", section: qsTr("DEVICE") },
        { key: "buttonStatus", label: qsTr("Button Status") },
        { key: "smartPause", label: qsTr("Smart Pause — By taking headset off") },
        { key: "factoryReset", label: qsTr("Factory Reset") }
    ]
    readonly property var application: [
        { sectionKey: "softphone", section: qsTr("SOFTPHONE") },
        { key: "defaultSoftphone", label: qsTr("Default Softphone") },
        { sectionKey: "language", section: qsTr("LANGUAGE") },
        { key: "language", label: qsTr("Language") }
    ]
}
