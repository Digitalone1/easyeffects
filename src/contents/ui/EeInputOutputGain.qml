import "Common.js" as Common
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: control

    required property var pluginDB
    property alias inputLevelLeft: inputLevelLeft.value
    property alias inputLevelRight: inputLevelRight.value
    property alias outputLevelLeft: outputLevelLeft.value
    property alias outputLevelRight: outputLevelRight.value
    readonly property real radius: 2.5 * Kirigami.Units.gridUnit

    implicitHeight: column.implicitHeight
    implicitWidth: column.implicitWidth

    ColumnLayout {
        id: column

        anchors.fill: parent

        Kirigami.CardsLayout {
            id: grid

            Layout.fillWidth: true

            RowLayout {
                EeSpinBox {
                    id: inputGain

                    horizontalPadding: 0
                    verticalPadding: 0
                    label: i18n("Input")
                    labelFillWidth: false
                    labelAbove: true
                    spinboxLayoutFillWidth: true
                    from: pluginDB.getMinValue("inputGain")
                    to: pluginDB.getMaxValue("inputGain")
                    value: pluginDB.inputGain
                    decimals: 0
                    stepSize: 1
                    unit: "dB"
                    boxWidth: 5 * Kirigami.Units.gridUnit
                    onValueModified: (v) => {
                        pluginDB.inputGain = v;
                    }
                }

                EeCircularProgress {
                    id: inputLevelLeft

                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    implicitWidth: control.radius
                    implicitHeight: control.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

                EeCircularProgress {
                    id: inputLevelRight

                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    implicitWidth: control.radius
                    implicitHeight: control.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

            }

            RowLayout {
                EeSpinBox {
                    id: outputGain

                    horizontalPadding: 0
                    verticalPadding: 0
                    label: i18n("Output")
                    labelFillWidth: true
                    labelAbove: true
                    spinboxLayoutFillWidth: true
                    from: pluginDB.getMinValue("outputGain")
                    to: pluginDB.getMaxValue("outputGain")
                    value: pluginDB.outputGain
                    decimals: 0
                    stepSize: 1
                    unit: "dB"
                    boxWidth: 5 * Kirigami.Units.gridUnit
                    onValueModified: (v) => {
                        pluginDB.outputGain = v;
                    }
                }

                EeCircularProgress {
                    id: outputLevelLeft

                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    implicitWidth: control.radius
                    implicitHeight: control.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

                EeCircularProgress {
                    id: outputLevelRight

                    Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                    implicitWidth: control.radius
                    implicitHeight: control.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

            }

        }

    }

}
