import QtQuick

Loader {
    required property bool selected
    property bool visited: false
    active: selected || visited
    onSelectedChanged: if (selected) visited = true
    Component.onCompleted: if (selected) visited = true
}
