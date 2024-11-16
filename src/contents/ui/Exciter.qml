import "Common.js" as Common
import EEdbm
import EEtagsPluginName
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.ScrollablePage {
    id: exciterPage

    required property var name
    required property var pluginDB
    required property var pipelineInstance
    property var pluginBackend

    function updateMeters() {
        if (!pluginBackend)
            return;

        inputOutputLevels.inputLevelLeft = pluginBackend.getInputLevelLeft();
        inputOutputLevels.inputLevelRight = pluginBackend.getInputLevelRight();
        inputOutputLevels.outputLevelLeft = pluginBackend.getOutputLevelLeft();
        inputOutputLevels.outputLevelRight = pluginBackend.getOutputLevelRight();
        harmonicsLevel.value = pluginBackend.getHarmonicsLevel();
    }

    Component.onCompleted: {
        pluginBackend = pipelineInstance.getPluginInstance(name);
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Blend Harmonics")
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 2 * cardLayout.maximumColumnWidth

            Controls.Label {
                Layout.alignment: Qt.AlignLeft
                text: i18n("3rd")
            }

            Controls.Slider {
                id: blend

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                orientation: Qt.Horizontal
                snapMode: Controls.Slider.SnapAlways
                value: pluginDB.blend
                from: pluginDB.getMinValue("blend")
                to: pluginDB.getMaxValue("blend")
                stepSize: 1
                onValueChanged: () => {
                    if (value !== pluginDB.blend)
                        pluginDB.blend = value;

                }
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("2rd")
            }

        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: blend.value
        }

        Kirigami.CardsLayout {
            id: cardLayout

            Layout.fillWidth: true

            Kirigami.Card {
                id: cardControls

                header: Kirigami.Heading {
                    text: i18n("Controls")
                    level: 2
                }

                contentItem: ColumnLayout {
                    EeSpinBox {
                        id: amount

                        label: i18n("Amount")
                        from: pluginDB.getMinValue("amount")
                        to: pluginDB.getMaxValue("amount")
                        value: pluginDB.amount
                        decimals: 1
                        stepSize: 0.1
                        unit: "dB"
                        onValueModified: (v) => {
                            pluginDB.amount = v;
                        }
                    }

                    EeSpinBox {
                        id: harmonics

                        label: i18n("Harmonics")
                        from: pluginDB.getMinValue("harmonics")
                        to: pluginDB.getMaxValue("harmonics")
                        value: pluginDB.harmonics
                        decimals: 1
                        stepSize: 0.1
                        onValueModified: (v) => {
                            pluginDB.harmonics = v;
                        }
                    }

                    EeSpinBox {
                        id: scope

                        label: i18n("Scope")
                        from: pluginDB.getMinValue("scope")
                        to: pluginDB.getMaxValue("scope")
                        value: pluginDB.scope
                        decimals: 0
                        stepSize: 1
                        unit: "Hz"
                        onValueModified: (v) => {
                            pluginDB.scope = v;
                        }
                    }

                    EeSwitch {
                        id: ceilActive

                        label: i18n("Ceil Active")
                        isChecked: pluginDB.ceilActive
                        onCheckedChanged: {
                            if (isChecked !== pluginDB.ceilActive)
                                pluginDB.ceilActive = isChecked;

                        }
                    }

                    EeSpinBox {
                        id: ceil

                        label: i18n("Ceil")
                        from: pluginDB.getMinValue("ceil")
                        to: pluginDB.getMaxValue("ceil")
                        value: pluginDB.ceil
                        decimals: 0
                        stepSize: 1
                        unit: "Hz"
                        enabled: ceilActive.isChecked
                        onValueModified: (v) => {
                            pluginDB.ceil = v;
                        }
                    }

                    EeProgressBar {
                        id: harmonicsLevel

                        label: i18n("Harmonics")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0
                    }

                }

            }

        }

    }

    header: EeInputOutputGain {
        id: inputOutputLevels

        pluginDB: exciterPage.pluginDB
    }

    footer: RowLayout {
        Controls.Label {
            text: i18n("Using") + EEtagsPluginPackage.calf
            horizontalAlignment: Qt.AlignLeft
            verticalAlignment: Qt.AlignVCenter
            Layout.fillWidth: false
            Layout.leftMargin: Kirigami.Units.smallSpacing
            color: Kirigami.Theme.disabledTextColor
        }

        Kirigami.ActionToolBar {
            Layout.margins: Kirigami.Units.smallSpacing
            alignment: Qt.AlignRight
            position: Controls.ToolBar.Footer
            flat: true
            actions: [
                Kirigami.Action {
                    text: i18n("Show Native Window")
                    icon.name: "window-duplicate-symbolic"
                    enabled: EEdbm.main.showNativePluginUi
                    checkable: true
                    checked: pluginBackend.hasNativeUi()
                    onTriggered: {
                        if (checked)
                            pluginBackend.show_native_ui();
                        else
                            pluginBackend.close_native_ui();
                    }
                },
                Kirigami.Action {
                    text: i18n("Listen")
                    icon.name: "audio-headset-symbolic"
                    checkable: true
                    checked: pluginDB.listen
                    onTriggered: {
                        if (pluginDB.listen != checked)
                            pluginDB.listen = checked;

                    }
                },
                Kirigami.Action {
                    text: i18n("Reset")
                    icon.name: "edit-reset-symbolic"
                    onTriggered: {
                        pluginBackend.reset();
                    }
                }
            ]
        }

    }

}
