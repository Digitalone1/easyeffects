import "Common.js" as Common
import QtQuick
import org.kde.kirigami as Kirigami
import ee.database as DB

Rectangle {
    id: root

    property real value: 0.5
    property real from: 0
    property real to: 1
    property int decimals: 2
    property bool convertDecibelToLinear: false
    property bool topToBottom: false
    readonly property real dbFrom: Common.dbToLinear(from)
    readonly property real dbTo: Common.dbToLinear(to)
    readonly property real clampedValue: Common.clamp(value, from, to)
    readonly property real displayValue: root.topToBottom === false ? (root.clampedValue > sampleTimer.value ? root.clampedValue : sampleTimer.value) : (root.clampedValue < sampleTimer.value ? root.clampedValue : sampleTimer.value)

    implicitWidth: valueLabel.implicitWidth + Kirigami.Units.largeSpacing
    implicitHeight: valueLabel.implicitWidth + Kirigami.Units.largeSpacing
    radius: Kirigami.Units.largeSpacing
    color: Kirigami.Theme.neutralBackgroundColor
    border.color: Kirigami.Theme.activeBackgroundColor
    border.width: 2
    Kirigami.Theme.colorSet: Kirigami.Theme.Button
    Kirigami.Theme.inherit: false
    clip: true

    Rectangle {
        id: levelRect

        width: parent.width
        height: parent.height
        radius: Kirigami.Units.largeSpacing
        color: Kirigami.Theme.alternateBackgroundColor
        border.width: 0

        transform: Scale {
            yScale: {
                if (root.convertDecibelToLinear)
                    root.topToBottom === false ? (Common.dbToLinear(root.clampedValue) - root.dbFrom) / (root.dbTo - root.dbFrom) : (Common.dbToLinear(root.clampedValue) - root.dbTo) / (root.dbFrom - root.dbTo);
                else
                    root.topToBottom === false ? (root.clampedValue - root.from) / (root.to - root.from) : (root.clampedValue - root.to) / (root.from - root.to);
            }

            origin.y: root.topToBottom === false ? levelRect.height : 0
        }

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    Rectangle {
        id: histRect

        height: Kirigami.Units.smallSpacing * 0.5
        width: parent.height
        color: Kirigami.Theme.positiveTextColor

        transform: Translate {
            y: {
                if (root.convertDecibelToLinear)
                    root.height * (1.0 - (Common.dbToLinear(root.displayValue) - root.dbFrom) / (root.dbTo - root.dbFrom));
                else
                    root.height * (1.0 - (root.displayValue - root.from) / (root.to - root.from));
            }
        }
    }

    Text {
        id: valueLabel

        anchors.centerIn: parent
        text: Number(root.displayValue).toLocaleString(Qt.locale(), 'f', root.decimals)
        color: Kirigami.Theme.textColor
    }

    Timer {
        id: sampleTimer

        property real value: root.clampedValue

        interval: DB.Manager.main.levelMetersLabelTimer
        repeat: true
        running: root.visible

        onTriggered: {
            value = root.clampedValue;
        }
    }
}
