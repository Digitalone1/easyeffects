import "Common.js" as Common
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import ee.database as DB
import ee.pipewire as PW
import ee.tags.plugin.name as TagsPluginName// qmllint disable
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.ScrollablePage {
    id: limiterPage

    required property var name
    required property var pluginDB
    required property var pipelineInstance
    property var pluginBackend

    function updateMeters() {
        if (!pluginBackend)
            return;

        inputOutputLevels.inputLevelLeft = limiterPage.pluginBackend.getInputLevelLeft();
        inputOutputLevels.inputLevelRight = limiterPage.pluginBackend.getInputLevelRight();
        inputOutputLevels.outputLevelLeft = limiterPage.pluginBackend.getOutputLevelLeft();
        inputOutputLevels.outputLevelRight = limiterPage.pluginBackend.getOutputLevelRight();
        gainLevelLeft.value = limiterPage.pluginBackend.getGainLevelLeft();
        gainLevelRight.value = limiterPage.pluginBackend.getGainLevelRight();
        sideChainLevelLeft.value = limiterPage.pluginBackend.getSideChainLevelLeft();
        sideChainLevelRight.value = limiterPage.pluginBackend.getSideChainLevelRight();
    }

    Component.onCompleted: {
        pluginBackend = pipelineInstance.getPluginInstance(name);
    }

    ColumnLayout {
        Kirigami.CardsLayout {
            id: cardLayout

            maximumColumns: 5
            minimumColumnWidth: Kirigami.Units.gridUnit * 16
            uniformCellWidths: true

            Kirigami.Card {
                id: cardMode

                contentItem: Column {
                    FormCard.FormComboBoxDelegate {
                        id: mode

                        text: i18n("Mode") // qmllint disable
                        displayMode: FormCard.FormComboBoxDelegate.ComboBox
                        currentIndex: limiterPage.pluginDB.mode
                        editable: false
                        model: [i18n("Herm Thin"), i18n("Herm Wide"), i18n("Herm Tail"), i18n("Herm Duck"), i18n("Exp Thin"), i18n("Exp Wide"), i18n("Exp Tail"), i18n("Exp Duck"), i18n("Line Thin"), i18n("Line Wide"), i18n("Line Tail"), i18n("Line Duck")]// qmllint disable
                        onActivated: idx => {
                            limiterPage.pluginDB.mode = idx;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    FormCard.FormComboBoxDelegate {
                        id: oversampling

                        text: i18n("Oversampling") // qmllint disable
                        displayMode: FormCard.FormComboBoxDelegate.ComboBox
                        currentIndex: limiterPage.pluginDB.oversampling
                        editable: false
                        model: [i18n("None"), i18n("Half x2/16 bit"), i18n("Half x2/24 bit"), i18n("Half x3/16 bit"), i18n("Half x3/24 bit"), i18n("Half x4/16 bit"), i18n("Half x4/24 bit"), i18n("Half x6/16 bit"), i18n("Half x6/24 bit"), i18n("Half x8/16 bit"), i18n("Half x8/24 bit"), i18n("Full x2/16 bit"), i18n("Full x2/24 bit"), i18n("Full x3/16 bit"), i18n("Full x3/24 bit"), i18n("Full x4/16 bit"), i18n("Full x4/24 bit"), i18n("Full x6/16 bit"), i18n("Full x6/24 bit"), i18n("Full x8/16 bit"), i18n("Full x8/24 bit")]// qmllint disable
                        onActivated: idx => {
                            limiterPage.pluginDB.oversampling = idx;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    FormCard.FormComboBoxDelegate {
                        id: dithering

                        text: i18n("Dithering") // qmllint disable
                        displayMode: FormCard.FormComboBoxDelegate.ComboBox
                        currentIndex: limiterPage.pluginDB.dithering
                        editable: false
                        model: [i18n("None"), i18n("7 bit"), i18n("8 bit"), i18n("11 bit"), i18n("12 bit"), i18n("15 bit"), i18n("16 bit"), i18n("23 bit"), i18n("24 bit")]// qmllint disable
                        onActivated: idx => {
                            limiterPage.pluginDB.dithering = idx;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }
                }
            }

            Kirigami.Card {
                id: cardLimiter

                header: Kirigami.Heading {
                    text: i18n("Limiter") // qmllint disable
                    level: 2
                }

                contentItem: Column {
                    EeSpinBox {
                        id: threshold

                        label: i18n("Threshold") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("threshold")
                        to: limiterPage.pluginDB.getMaxValue("threshold")
                        value: limiterPage.pluginDB.threshold
                        decimals: 2
                        stepSize: 0.01
                        unit: "dB"
                        onValueModified: v => {
                            limiterPage.pluginDB.threshold = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    EeSpinBox {
                        id: attack

                        label: i18n("Attack") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("attack")
                        to: limiterPage.pluginDB.getMaxValue("attack")
                        value: limiterPage.pluginDB.attack
                        decimals: 2
                        stepSize: 0.01
                        unit: "ms"
                        onValueModified: v => {
                            limiterPage.pluginDB.attack = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    EeSpinBox {
                        id: release

                        label: i18n("Release") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("release")
                        to: limiterPage.pluginDB.getMaxValue("release")
                        value: limiterPage.pluginDB.release
                        decimals: 2
                        stepSize: 0.01
                        unit: "ms"
                        onValueModified: v => {
                            limiterPage.pluginDB.release = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    EeSpinBox {
                        id: stereoLink

                        label: i18n("Stereo Link") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("stereoLink")
                        to: limiterPage.pluginDB.getMaxValue("stereoLink")
                        value: limiterPage.pluginDB.stereoLink
                        decimals: 1
                        stepSize: 0.1
                        unit: "%"
                        onValueModified: v => {
                            limiterPage.pluginDB.stereoLink = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }
                }
            }

            Kirigami.Card {
                id: cardSideChain

                header: Kirigami.Heading {
                    text: i18n("Sidechain") // qmllint disable
                    level: 2
                }

                contentItem: Column {
                    id: cardSideChainColumn

                    GridLayout {
                        columns: 2
                        uniformCellWidths: true

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        FormCard.FormComboBoxDelegate {
                            id: sidechainType

                            Layout.columnSpan: 2
                            text: i18n("Type") // qmllint disable
                            displayMode: FormCard.FormComboBoxDelegate.ComboBox
                            currentIndex: limiterPage.pluginDB.sidechainType
                            editable: false
                            model: [i18n("Internal"), i18n("External"), i18n("Link")]// qmllint disable
                            onActivated: idx => {
                                limiterPage.pluginDB.sidechainType = idx;
                            }
                        }

                        FormCard.FormComboBoxDelegate {
                            id: comboSideChainInputDevice

                            Layout.columnSpan: 2
                            text: i18n("Input Device") // qmllint disable
                            displayMode: FormCard.FormComboBoxDelegate.ComboBox
                            editable: false
                            model: PW.ModelNodes
                            textRole: "description"
                            enabled: sidechainType.currentIndex === 1
                            currentIndex: {
                                for (let n = 0; n < PW.ModelNodes.rowCount(); n++) {
                                    if (PW.ModelNodes.getNodeName(n) === limiterPage.pluginDB.sidechainInputDevice)
                                        return n;
                                }
                                return 0;
                            }
                            onActivated: idx => {
                                let selectedName = PW.ModelNodes.getNodeName(idx);
                                if (selectedName !== limiterPage.pluginDB.sidechainInputDevice)
                                    limiterPage.pluginDB.sidechainInputDevice = selectedName;
                            }
                        }

                        EeSpinBox {
                            id: sidechainPreamp

                            label: i18n("Preamp") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("sidechainPreamp")
                            to: limiterPage.pluginDB.getMaxValue("sidechainPreamp")
                            value: limiterPage.pluginDB.sidechainPreamp
                            decimals: 2 // Required to show "-inf"
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.sidechainPreamp = v;
                            }
                        }

                        EeSpinBox {
                            id: lookahead

                            label: i18n("Lookahead") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("lookahead")
                            to: limiterPage.pluginDB.getMaxValue("lookahead")
                            value: limiterPage.pluginDB.lookahead
                            decimals: 2
                            stepSize: 0.01
                            unit: "ms"
                            onValueModified: v => {
                                limiterPage.pluginDB.lookahead = v;
                            }
                        }
                    }
                }
            }

            Kirigami.Card {
                id: cardPreMix

                header: Kirigami.Heading {
                    text: i18n("Pre-Mix") // qmllint disable
                    level: 2
                }

                contentItem: Column {
                    GridLayout {
                        columns: 2
                        uniformCellWidths: true

                        anchors {
                            left: parent.left
                            right: parent.right
                        }

                        EeSpinBox {
                            label: i18n("Input to Sidechain") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("inputToSidechain")
                            to: limiterPage.pluginDB.getMaxValue("inputToSidechain")
                            value: limiterPage.pluginDB.inputToSidechain
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.inputToSidechain = v;
                            }
                        }

                        EeSpinBox {
                            label: i18n("Input to Link") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("inputToLink")
                            to: limiterPage.pluginDB.getMaxValue("inputToLink")
                            value: limiterPage.pluginDB.inputToLink
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.inputToLink = v;
                            }
                        }

                        EeSpinBox {
                            label: i18n("Sidechain to Input") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("sidechainToInput")
                            to: limiterPage.pluginDB.getMaxValue("sidechainToInput")
                            value: limiterPage.pluginDB.sidechainToInput
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.sidechainToInput = v;
                            }
                        }

                        EeSpinBox {
                            label: i18n("Sidechain to Link") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("sidechainToLink")
                            to: limiterPage.pluginDB.getMaxValue("sidechainToLink")
                            value: limiterPage.pluginDB.sidechainToLink
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.sidechainToLink = v;
                            }
                        }

                        EeSpinBox {
                            label: i18n("Link to Sidechain") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("linkToSidechain")
                            to: limiterPage.pluginDB.getMaxValue("linkToSidechain")
                            value: limiterPage.pluginDB.linkToSidechain
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.linkToSidechain = v;
                            }
                        }

                        EeSpinBox {
                            label: i18n("Link to Input") // qmllint disable
                            labelAbove: true
                            spinboxLayoutFillWidth: true
                            from: limiterPage.pluginDB.getMinValue("linkToInput")
                            to: limiterPage.pluginDB.getMaxValue("linkToInput")
                            value: limiterPage.pluginDB.linkToInput
                            decimals: 2
                            stepSize: 0.01
                            unit: "dB"
                            minusInfinityMode: true
                            onValueModified: v => {
                                limiterPage.pluginDB.linkToInput = v;
                            }
                        }
                    }
                }
            }

            Kirigami.Card {
                id: cardALR

                enabled: limiterPage.pluginDB.alr

                header: Kirigami.Heading {
                    text: i18n("Automatic Level") // qmllint disable
                    level: 2
                }

                contentItem: Column {
                    EeSpinBox {
                        id: alrAttack

                        label: i18n("Attack") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("alrAttack")
                        to: limiterPage.pluginDB.getMaxValue("alrAttack")
                        value: limiterPage.pluginDB.alrAttack
                        decimals: 2
                        stepSize: 0.01
                        unit: "ms"
                        onValueModified: v => {
                            limiterPage.pluginDB.alrAttack = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    EeSpinBox {
                        id: alrRelease

                        label: i18n("Release") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("alrRelease")
                        to: limiterPage.pluginDB.getMaxValue("alrRelease")
                        value: limiterPage.pluginDB.alrRelease
                        decimals: 1
                        stepSize: 0.1
                        unit: "ms"
                        onValueModified: v => {
                            limiterPage.pluginDB.alrRelease = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }

                    EeSpinBox {
                        id: alrKnee

                        label: i18n("Knee") // qmllint disable
                        spinboxMaximumWidth: Kirigami.Units.gridUnit * 6
                        from: limiterPage.pluginDB.getMinValue("alrKnee")
                        to: limiterPage.pluginDB.getMaxValue("alrKnee")
                        value: limiterPage.pluginDB.alrKnee
                        decimals: 2
                        stepSize: 0.01
                        unit: "dB"
                        onValueModified: v => {
                            limiterPage.pluginDB.alrKnee = v;
                        }

                        anchors {
                            left: parent.left
                            right: parent.right
                        }
                    }
                }
            }
        }

        Kirigami.Card {
            id: cardLevels

            Layout.fillWidth: false
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.largeSpacing

            contentItem: GridLayout {
                id: levelGridLayout

                readonly property real radius: 2.5 * Kirigami.Units.gridUnit

                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing
                columns: 4
                rows: 3

                Controls.Label {
                    Layout.columnSpan: 2
                    Layout.alignment: Qt.AlignHCenter
                    topPadding: Kirigami.Units.smallSpacing
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("Gain") // qmllint disable
                }

                Controls.Label {
                    Layout.columnSpan: 2
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.smallSpacing
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("Sidechain") // qmllint disable
                }

                EeAudioLevel {
                    id: gainLevelLeft

                    Layout.alignment: Qt.AlignBottom
                    implicitWidth: levelGridLayout.radius
                    implicitHeight: levelGridLayout.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                    topToBottom: true
                }

                EeAudioLevel {
                    id: gainLevelRight

                    Layout.alignment: Qt.AlignBottom
                    implicitWidth: levelGridLayout.radius
                    implicitHeight: levelGridLayout.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                    topToBottom: true
                }

                EeAudioLevel {
                    id: sideChainLevelLeft

                    Layout.alignment: Qt.AlignBottom
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    implicitWidth: levelGridLayout.radius
                    implicitHeight: levelGridLayout.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

                EeAudioLevel {
                    id: sideChainLevelRight

                    Layout.alignment: Qt.AlignBottom
                    implicitWidth: levelGridLayout.radius
                    implicitHeight: levelGridLayout.radius
                    from: Common.minimumDecibelLevel
                    to: 0
                    value: 0
                    decimals: 0
                    convertDecibelToLinear: true
                }

                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("L") // qmllint disable
                }

                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("R") // qmllint disable
                }

                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: Kirigami.Units.gridUnit
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("L") // qmllint disable
                }

                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: i18n("R") // qmllint disable
                }
            }
        }
    }

    header: EeInputOutputGain {
        id: inputOutputLevels

        pluginDB: limiterPage.pluginDB
    }

    footer: RowLayout {
        Controls.Label {
            text: i18n("Using %1", `<b>${TagsPluginName.Package.lsp}</b>`) // qmllint disable
            textFormat: Text.RichText
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
                    text: i18n("Show Native Window") // qmllint disable
                    icon.name: "window-duplicate-symbolic"
                    enabled: DB.Manager.main.showNativePluginUi
                    checkable: true
                    checked: limiterPage.pluginBackend ? limiterPage.pluginBackend.hasNativeUi() : false
                    onTriggered: {
                        if (checked)
                            limiterPage.pluginBackend.showNativeUi();
                        else
                            limiterPage.pluginBackend.closeNativeUi();
                    }
                },
                Kirigami.Action {
                    text: i18n("Gain Boost") // qmllint disable
                    icon.name: "usermenu-up-symbolic"
                    checkable: true
                    checked: limiterPage.pluginDB.gainBoost
                    onTriggered: {
                        if (limiterPage.pluginDB.gainBoost != checked)
                            limiterPage.pluginDB.gainBoost = checked;
                    }
                },
                Kirigami.Action {
                    text: i18n("Automatic Level") // qmllint disable
                    icon.name: "usermenu-up-symbolic"
                    checkable: true
                    checked: limiterPage.pluginDB.alr
                    onTriggered: {
                        if (limiterPage.pluginDB.alr != checked)
                            limiterPage.pluginDB.alr = checked;
                    }
                },
                Kirigami.Action {
                    text: i18n("Reset") // qmllint disable
                    icon.name: "edit-reset-symbolic"
                    onTriggered: {
                        limiterPage.pluginBackend.reset();
                    }
                }
            ]
        }
    }
}
