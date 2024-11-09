/*
  It is really bizarre that qml does not have a dedicated double spinbox... It is necessary to do a hack
  solution based on the int spinbox... Our custom spinbox code was inspired by:

https://doc.qt.io/Qt-6/qml-qtquick-controls-spinbox.html
https://github.com/LeonnardoVerol/example-double-spinbox-component-qt-qml/blob/main/src/ui/DoubleSpinBox.qml
https://github.com/Zren/plasma-applet-lib/blob/master/package/contents/ui/libconfig/SpinBox.qml
https://github.com/KDE/kirigami-addons/blob/master/src/formcard/FormSpinBoxDelegate.qml
https://bugreports.qt.io/browse/QTBUG-67349

 */

import "Common.js" as Common
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

FormCard.AbstractFormDelegate {
    id: control

    property int decimals: 2
    property real value: 0
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real pageSteps: 10
    property bool editable: true
    property string label: ""
    property string subtitle: ""
    property string unit: ""
    property alias displayText: spinbox.displayText
    property int boxWidth: 10 * Kirigami.Units.gridUnit
    property bool labelAbove: false
    property bool labelFillWidth: true
    property bool spinboxLayoutFillWidth: false
    property int elide: Text.ElideRight
    property int wrapMode: Text.Wrap
    property int spinboxAlignment: Qt.AlignRight
    property int labelAlignment: Qt.AlignLeft
    property bool minusInfinityMode: false
    property int maximumLineCount: 2

    signal valueModified(real value)

    focusPolicy: Kirigami.Settings.isMobile ? Qt.StrongFocus : Qt.NoFocus
    onClicked: spinbox.forceActiveFocus()
    Keys.onPressed: (event) => {
        if (event.key == Qt.Key_PageUp) {
            control.valueModified(control.value + pageSteps * stepSize);
            event.accepted = true;
        } else if (event.key === Qt.Key_PageDown) {
            control.valueModified(control.value - pageSteps * stepSize);
            event.accepted = true;
        }
    }

    contentItem: GridLayout {
        columns: labelAbove === false ? 2 : 1
        rows: labelAbove === false ? 1 : 2
        rowSpacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Label {
                id: label

                Layout.fillWidth: labelFillWidth
                text: control.label
                elide: control.elide
                color: control.enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                wrapMode: control.wrapMode
                maximumLineCount: control.maximumLineCount
                horizontalAlignment: labelAlignment
            }

            Label {
                id: subtitle

                Layout.fillWidth: labelFillWidth
                text: control.subtitle
                elide: control.elide
                color: Kirigami.Theme.disabledTextColor
                wrapMode: control.wrapMode
                maximumLineCount: control.maximumLineCount
                horizontalAlignment: labelAlignment
                visible: !Common.isEmpty(control.subtitle)
            }

        }

        SpinBox {
            id: spinbox

            readonly property real decimalFactor: Math.pow(10, control.decimals)

            function decimalToInt(decimal) {
                return decimal * decimalFactor;
            }

            Layout.fillWidth: control.spinboxLayoutFillWidth
            Layout.alignment: spinboxAlignment
            implicitWidth: control.boxWidth
            focusPolicy: control.focusPolicy
            wheelEnabled: true
            onValueModified: {
                control.valueModified(spinbox.value * 1 / spinbox.decimalFactor);
            }
            stepSize: spinbox.decimalToInt(control.stepSize)
            value: spinbox.decimalToInt(control.value)
            to: spinbox.decimalToInt(control.to)
            from: spinbox.decimalToInt(control.from)
            editable: control.editable
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            textFromValue: (value, locale) => {
                let unit_str = (Common.isEmpty(unit)) ? "" : " " + unit;
                locale.numberOptions = Locale.OmitGroupSeparator;
                let decimalValue = value / spinbox.decimalFactor;
                if (control.minusInfinityMode === true && decimalValue <= control.from) {
                    textInputSpinBox.text = "-inf";
                    return "-inf";
                }
                let t = Number(decimalValue).toLocaleString(locale, 'f', control.decimals) + unit_str;
                textInputSpinBox.text = t;
                return t;
            }
            valueFromText: (text, locale) => {
                if (text === "-inf") {
                    let v = control.from * spinbox.decimalFactor;
                    return Math.round(v);
                }
                let re = /-?\d*[.,]?\d*/;
                let regex_result = re.exec(text);
                let v = Number.fromLocaleString(locale, regex_result[0]) * spinbox.decimalFactor;
                v = (!isNaN(v)) ? Math.round(v) : spinbox.value;
                return v;
            }

            validator: RegularExpressionValidator {
                regularExpression: /^-inf|^-?\d*[.,]?\d*/
            }

            contentItem: TextInput {
                id: textInputSpinBox

                z: 2
                font: spinbox.font
                color: control.enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                selectionColor: Kirigami.Theme.highlightColor
                readOnly: !spinbox.editable
                validator: spinbox.validator
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

        }

    }

}
