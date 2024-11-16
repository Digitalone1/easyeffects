import "Common.js" as Common
import EEdbm
import EEtagsPluginName
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.ScrollablePage {
    id: autogainPage

    required property var name
    required property var pluginDB
    required property var pipelineInstance
    property var pluginBackend

    function updateMeters() {
        if (!pluginBackend)
            return ;

        momentary.value = pluginBackend.getMomentaryLevel();
        shortterm.value = pluginBackend.getShorttermLevel();
        integrated.value = pluginBackend.getIntegratedLevel();
        relative.value = pluginBackend.getRelativeLevel();
        range.value = pluginBackend.getRangeLevel();
        loudness.value = pluginBackend.getLoudnessLevel();
        outputGain.value = pluginBackend.getOutputGainLevel();
        inputOutputLevels.inputLevelLeft = pluginBackend.getInputLevelLeft();
        inputOutputLevels.inputLevelRight = pluginBackend.getInputLevelRight();
        inputOutputLevels.outputLevelLeft = pluginBackend.getOutputLevelLeft();
        inputOutputLevels.outputLevelRight = pluginBackend.getOutputLevelRight();
    }

    Component.onCompleted: {
        pluginBackend = pipelineInstance.getPluginInstance(name);
    }

    ColumnLayout {
        Kirigami.CardsLayout {
            id: cardLayout

            uniformCellWidths: true

            Kirigami.Card {
                id: cardControls

                implicitWidth: cardLayout.maximumColumnWidth

                header: Kirigami.Heading {
                    text: i18n("Controls")
                    level: 2
                }

                contentItem: Column {
                    FormCard.FormComboBoxDelegate {
                        id: reference

                        text: i18n("Reference")
                        displayMode: FormCard.FormComboBoxDelegate.ComboBox
                        currentIndex: pluginDB.reference
                        editable: false
                        model: [i18n("Momentary"), i18n("Short-Term"), i18n("Integrated"), i18n("Geometric Mean (MSI)"), i18n("Geometric Mean (MS)"), i18n("Geometric Mean (MI)"), i18n("Geometric Mean (SI)")]
                        onActivated: (idx) => {
                            pluginDB.reference = idx;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeSpinBox {
                        id: target

                        label: i18n("Target")
                        from: pluginDB.getMinValue("target")
                        to: pluginDB.getMaxValue("target")
                        value: pluginDB.target
                        decimals: 0
                        stepSize: 1
                        unit: "dB"
                        onValueModified: (v) => {
                            pluginDB.target = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeSpinBox {
                        id: silenceThreshold

                        label: i18n("Silence")
                        from: pluginDB.getMinValue("silenceThreshold")
                        to: pluginDB.getMaxValue("silenceThreshold")
                        value: pluginDB.silenceThreshold
                        decimals: 0
                        stepSize: 1
                        unit: "dB"
                        onValueModified: (v) => {
                            pluginDB.silenceThreshold = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeSpinBox {
                        id: maximumHistory

                        label: i18n("Maximum History")
                        from: pluginDB.getMinValue("maximumHistory")
                        to: pluginDB.getMaxValue("maximumHistory")
                        value: pluginDB.maximumHistory
                        decimals: 0
                        stepSize: 1
                        unit: "s"
                        onValueModified: (v) => {
                            pluginDB.maximumHistory = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                }

            }

            Kirigami.Card {
                id: cardLevels

                implicitWidth: cardLayout.maximumColumnWidth

                header: Kirigami.Heading {
                    text: i18n("Loudness")
                    level: 2
                }

                contentItem: Column {
                    spacing: Kirigami.Units.gridUnit

                    EeProgressBar {
                        id: momentary

                        label: i18n("Momentary")
                        unit: i18n("LUFS")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: shortterm

                        label: i18n("Short-Term")
                        unit: i18n("LUFS")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: integrated

                        label: i18n("Integrated")
                        unit: i18n("LUFS")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: relative

                        label: i18n("Relative")
                        unit: i18n("LUFS")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: range

                        label: i18n("Range")
                        unit: i18n("LU")
                        from: 0
                        to: 50
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: loudness

                        label: i18n("Loudness")
                        unit: i18n("LUFS")
                        from: Common.minimumDecibelLevel
                        to: 10
                        value: 0
                        decimals: 0

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                    EeProgressBar {
                        id: outputGain

                        label: i18n("Output Gain")
                        unit: i18n("dB")
                        from: Common.minimumDecibelLevel
                        to: 20
                        value: 0
                        decimals: 1

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                    }

                }

            }

        }

    }

    header: EeInputOutputGain {
        id: inputOutputLevels

        pluginDB: autogainPage.pluginDB
    }

    footer: RowLayout {
        Controls.Label {
            text: i18n("Using") + EEtagsPluginPackage.ebur128
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
                    text: i18n("Reset History")
                    icon.name: "edit-clear-history-symbolic"
                    onTriggered: {
                        pluginBackend.resetHistory();
                    }
                },
                Kirigami.Action {
                    text: i18n("Reset Settings")
                    icon.name: "edit-reset-symbolic"
                    onTriggered: {
                        pluginBackend.reset();
                    }
                }
            ]
        }

    }

}
