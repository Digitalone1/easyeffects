import "Common.js" as Common
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls

Controls.ItemDelegate {
    id: delegate

    required property int index
    required property var bandDB
    required property var menu

    function toLocaleLabel(num, decimal, unit) {
        return Number(num).toLocaleString(Qt.locale(), 'f', decimal) + ` ${unit}`;
    }

    down: false
    hoverEnabled: false
    height: ListView.view.height

    contentItem: ColumnLayout {
        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: delegate.index + 1
            enabled: false
        }

        Controls.Button {
            id: menuButton

            Layout.alignment: Qt.AlignCenter
            icon.name: "emblem-system-symbolic"
            checkable: true
            checked: false
            onCheckedChanged: {
                delegate.menu.index = delegate.index;
                delegate.menu.menuButton = menuButton;
                if (checked)
                    delegate.menu.open();
                else
                    delegate.menu.close();
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: {
                const f = delegate.bandDB["band" + delegate.index + "Frequency"];
                if (f < 1000) {
                    return toLocaleLabel(f, 0, "Hz");
                } else {
                    return toLocaleLabel(f * 0.001, 1, "kHz");
                }
            }
            enabled: false
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Q") + " " + toLocaleLabel(delegate.bandDB["band" + delegate.index + "Q"], 2, "")
            enabled: false
        }

        Controls.Slider {
            id: gainSlider

            readonly property string bandName: "band" + delegate.index + "Gain"
            property real pageSteps: 10

            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true

            orientation: Qt.Vertical
            from: delegate.bandDB.getMinValue(bandName)
            to: delegate.bandDB.getMaxValue(bandName)
            value: delegate.bandDB[bandName]
            stepSize: 0.01
            enabled: true
            onMoved: {
                if (value != delegate.bandDB[bandName])
                    delegate.bandDB[bandName] = value;
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_PageUp) {
                    const v = value + pageSteps * stepSize;

                    delegate.bandDB[bandName] = Common.clamp(v, from, to);

                    event.accepted = true;
                } else if (event.key === Qt.Key_PageDown) {
                    const v = value - pageSteps * stepSize;

                    delegate.bandDB[bandName] = Common.clamp(v, from, to);

                    event.accepted = true;
                }
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: Number(gainSlider.value).toLocaleString(Qt.locale(), 'f', 2)
            enabled: false
        }
    }
}
