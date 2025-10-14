import "Common.js" as Common
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Controls.ItemDelegate {
    id: delegate

    required property int index
    required property real adaptiveIntensity
    required property var pluginDB
    required property var pluginBackend
    readonly property real frequency: pluginBackend.getBandFrequency(index)

    down: false
    hoverEnabled: false
    height: ListView.view.height

    Controls.Popup {
        id: menu

        parent: menuButton
        focus: true
        x: Math.round((parent.width - width) / 2)
        y: parent.height
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        onClosed: {
            menuButton.checked = false;
        }

        contentItem: ColumnLayout {
            Controls.Button {
                Layout.alignment: Qt.AlignCenter
                text: i18n("Mute") // qmllint disable
                checkable: true
                checked: delegate.pluginDB["muteBand" + delegate.index]
                onCheckedChanged: {
                    if (checked != delegate.pluginDB["muteBand" + delegate.index])
                        delegate.pluginDB["muteBand" + delegate.index] = checked;
                }
            }

            Controls.Button {
                Layout.alignment: Qt.AlignCenter
                text: i18n("Bypass") // qmllint disable
                checkable: true
                checked: delegate.pluginDB["bypassBand" + delegate.index]
                onCheckedChanged: {
                    if (checked != delegate.pluginDB["bypassBand" + delegate.index])
                        delegate.pluginDB["bypassBand" + delegate.index] = checked;
                }
            }
        }
    }

    contentItem: ColumnLayout {
        Controls.Button {
            id: menuButton

            Layout.alignment: Qt.AlignCenter
            icon.name: "emblem-system-symbolic"
            checkable: true
            checked: false
            onCheckedChanged: {
                if (checked)
                    menu.open();
                else
                    menu.close();
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: frequency >= 1000 ? Common.toLocaleLabel(delegate.frequency / 1000, 1, "kHz") : Common.toLocaleLabel(delegate.frequency, 0, "Hz")
            enabled: false
        }

        Controls.Slider {
            id: intensitySlider

            property real pageSteps: 10

            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            orientation: Qt.Vertical
            from: delegate.pluginDB.getMinValue("intensityBand" + delegate.index)
            to: delegate.pluginDB.getMaxValue("intensityBand" + delegate.index)
            value: delegate.pluginDB["intensityBand" + delegate.index]
            stepSize: 1
            enabled: !delegate.pluginDB["muteBand" + delegate.index] && !delegate.pluginDB["bypassBand" + delegate.index]
            onMoved: {
                if (value != delegate.pluginDB["intensityBand" + delegate.index])
                    delegate.pluginDB["intensityBand" + delegate.index] = value;
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_PageUp) {
                    const v = value + pageSteps * stepSize;

                    delegate.pluginDB["intensityBand" + delegate.index] = Common.clamp(v, from, to);

                    event.accepted = true;
                } else if (event.key === Qt.Key_PageDown) {
                    const v = value - pageSteps * stepSize;

                    delegate.pluginDB["intensityBand" + delegate.index] = Common.clamp(v, from, to);

                    event.accepted = true;
                }
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: Number(intensitySlider.value).toLocaleString(Qt.locale(), 'f', 0)
            enabled: false
        }

        EeAudioLevel {
            readonly property real radius: 2.5 * Kirigami.Units.gridUnit

            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            implicitWidth: radius
            implicitHeight: radius
            from: delegate.pluginDB.getMinValue("intensityBand" + delegate.index)
            to: delegate.pluginDB.getMaxValue("intensityBand" + delegate.index)
            value: delegate.adaptiveIntensity
            decimals: 1
            visible: delegate.pluginDB.adaptiveIntensity
        }
    }
}
