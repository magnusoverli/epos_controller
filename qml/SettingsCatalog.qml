pragma Singleton
import QtQuick

QtObject {
    // Stable keys identify future capabilities; labels are presentation only.
    // This is an inventory, not defaults or device readback.
    readonly property var device: [
        { section: "CALL AND AUDIO" },
        { key: "ancMode", label: "ANC and Talkthrough" },
        { key: "adaptiveAnc", label: "Adaptive" },
        { key: "ancLevel", label: "ANC level" },
        { key: "sidetone", label: "Sidetone" },
        { key: "audioLimiter", label: "Audio Limiter" },
        { key: "answerOnBoomDown", label: "Answer Call — By moving boom arm down" },
        { key: "answerOnWear", label: "Answer Call — By putting headset on" },
        { key: "muteOnBoomUp", label: "Mute Call — By moving boom arm up" },
        { key: "muteOnRemoval", label: "Mute Call — By removing headset" },
        { key: "boomSide", label: "Boom Arm Side" },
        { key: "autoPause", label: "Auto Pause" },
        { section: "SOUNDS & NOTIFICATIONS" },
        { key: "prompts", label: "Tone & Voice Prompts" },
        { key: "promptLanguage", label: "Voice Prompt Language" },
        { key: "noPromptsDuringCalls", label: "No Prompts During Calls" },
        { key: "productRingtone", label: "Product Ringtone" },
        { key: "busylight", label: "Busylight" },
        { key: "muteNotification", label: "Mute Notification" },
        { key: "muteInterval", label: "Mute Interval" },
        { key: "talkWhileMuted", label: "Talk While Muted" },
        { section: "DEVICE" },
        { key: "buttonStatus", label: "Button Status" },
        { key: "smartPause", label: "Smart Pause — By taking headset off" },
        { key: "factoryReset", label: "Factory Reset" }
    ]
    readonly property var application: [
        { section: "SOFTPHONE" },
        { key: "defaultSoftphone", label: "Default Softphone" },
        { section: "LANGUAGE" },
        { key: "language", label: "Language" }
    ]
}
