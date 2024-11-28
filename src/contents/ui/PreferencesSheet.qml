import QtQml
import QtQuick.Controls as Controls
import QtQuick.Layouts
import ee.database as DB
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard

Kirigami.OverlaySheet {
    id: preferencesSheet

    parent: applicationWindow().overlay
    implicitHeight: parent.height < stack.currentItem.implicitHeight ? parent.height : stack.currentItem.implicitHeight + 2 * aboutSheet.header.implicitHeight
    implicitWidth: Kirigami.Units.gridUnit * 30
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
    focus: true
    title: i18n("Preferences")
    onVisibleChanged: {
        if (!preferencesSheet.visible) {
            while (stack.depth > 1)stack.pop()
            headerTitle.text = i18n("Preferences");
        }
    }

    Component {
        id: servicePage

        Kirigami.Page {
            ColumnLayout {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                FormCard.FormCard {
                    EeSwitch {
                        id: enableServiceMode

                        label: i18n("Enable Service Mode")
                        isChecked: DB.Manager.main.enableServiceMode
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.enableServiceMode)
                                DB.Manager.main.enableServiceMode = isChecked;

                        }
                    }

                    EeSwitch {
                        id: autostartOnLogin

                        label: i18n("Autostart on Login")
                        // isChecked: DB.Manager.main.autostartOnLogin
                        onCheckedChanged: {
                        }
                    }

                    EeSwitch {
                        id: showTrayIcon

                        label: i18n("Show the Tray Icon")
                        isChecked: DB.Manager.main.showTrayIcon && canUseSysTray
                        enabled: canUseSysTray
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.showTrayIcon)
                                DB.Manager.main.showTrayIcon = isChecked;

                        }
                    }

                }

            }

        }

    }

    Component {
        id: audioPage

        Kirigami.Page {
            ColumnLayout {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                FormCard.FormCard {
                    EeSwitch {
                        id: processAllOutputs

                        label: i18n("Process All Output Streams")
                        isChecked: DB.Manager.main.processAllOutputs
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.processAllOutputs)
                                DB.Manager.main.processAllOutputs = isChecked;

                        }
                    }

                    EeSwitch {
                        id: processAllInputs

                        label: i18n("Process All Input Streams")
                        isChecked: DB.Manager.main.processAllInputs
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.processAllInputs)
                                DB.Manager.main.processAllInputs = isChecked;

                        }
                    }

                    EeSwitch {
                        id: excludeMonitorStreams

                        label: i18n("Ignore Streams from Monitor of Devices")
                        isChecked: DB.Manager.main.excludeMonitorStreams
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.excludeMonitorStreams)
                                DB.Manager.main.excludeMonitorStreams = isChecked;

                        }
                    }

                    EeSwitch {
                        id: useCubicVolumes

                        label: i18n("Use Cubic Volume")
                        isChecked: DB.Manager.main.useCubicVolumes
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.useCubicVolumes)
                                DB.Manager.main.useCubicVolumes = isChecked;

                        }
                    }

                    EeSwitch {
                        id: inactivityTimerEnable

                        label: i18n("Enable the Inactivity Timeout")
                        isChecked: DB.Manager.main.inactivityTimerEnable
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.inactivityTimerEnable)
                                DB.Manager.main.inactivityTimerEnable = isChecked;

                        }
                    }

                    EeSpinBox {
                        id: inactivityTimeout

                        label: i18n("Inactivity Timeout")
                        from: DB.Manager.main.getMinValue("inactivityTimeout")
                        to: DB.Manager.main.getMaxValue("inactivityTimeout")
                        value: DB.Manager.main.inactivityTimeout
                        decimals: 0
                        stepSize: 1
                        unit: "s"
                        enabled: DB.Manager.main.inactivityTimerEnable
                        onValueModified: (v) => {
                            DB.Manager.main.inactivityTimeout = v;
                        }
                    }

                }

            }

        }

    }

    Component {
        id: spectrumPage

        Kirigami.Page {
            ColumnLayout {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                FormCard.FormHeader {
                    title: i18n("State")
                }

                FormCard.FormCard {
                    EeSwitch {
                        id: spectrumState

                        label: i18n("Enabled")
                        isChecked: DB.Manager.spectrum.state
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.spectrum.state)
                                DB.Manager.spectrum.state = isChecked;

                        }
                    }

                }

                FormCard.FormHeader {
                    title: i18n("Style")
                }

                FormCard.FormCard {
                    FormCard.FormComboBoxDelegate {
                        id: spectrumShape

                        text: i18n("Shape")
                        displayMode: FormCard.FormComboBoxDelegate.ComboBox
                        currentIndex: DB.Manager.spectrum.spectrumShape
                        editable: false
                        model: [i18n("Bars"), i18n("Lines"), i18n("Dots"), i18n("Area")]
                        onActivated: (idx) => {
                            if (idx !== DB.Manager.spectrum.spectrumShape)
                                DB.Manager.spectrum.spectrumShape = idx;

                        }
                    }

                    EeSwitch {
                        label: i18n("Enable OpenGL Acceleration")
                        isChecked: DB.Manager.spectrum.useOpenGL
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.spectrum.useOpenGL)
                                DB.Manager.spectrum.useOpenGL = isChecked;

                        }
                    }

                    EeSwitch {
                        id: dynamicYScale

                        label: i18n("Dynamic Scale")
                        isChecked: DB.Manager.spectrum.dynamicYScale
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.spectrum.dynamicYScale)
                                DB.Manager.spectrum.dynamicYScale = isChecked;

                        }
                    }

                    EeSpinBox {
                        id: nPoints

                        label: i18n("Points")
                        from: 2
                        to: 2048
                        value: DB.Manager.spectrum.nPoints
                        decimals: 0
                        stepSize: 1
                        onValueModified: (v) => {
                            DB.Manager.spectrum.nPoints = v;
                        }
                    }

                    EeSpinBox {
                        id: height

                        label: i18n("Height")
                        from: 100
                        to: 1000
                        value: DB.Manager.spectrum.height
                        decimals: 0
                        stepSize: 1
                        unit: "px"
                        onValueModified: (v) => {
                            DB.Manager.spectrum.height = v;
                        }
                    }

                }

                FormCard.FormHeader {
                    title: i18n("Frequency Range")
                }

                FormCard.FormCard {
                    EeSpinBox {
                        id: minimumFrequency

                        label: i18n("Minimum")
                        from: 20
                        to: 21900
                        value: DB.Manager.spectrum.minimumFrequency
                        decimals: 0
                        stepSize: 10
                        unit: "Hz"
                        onValueModified: (v) => {
                            DB.Manager.spectrum.minimumFrequency = v;
                        }
                    }

                    EeSpinBox {
                        id: maximumFrequency

                        label: i18n("Maximum")
                        from: 120
                        to: 22000
                        value: DB.Manager.spectrum.maximumFrequency
                        decimals: 0
                        stepSize: 10
                        unit: "Hz"
                        onValueModified: (v) => {
                            DB.Manager.spectrum.maximumFrequency = v;
                        }
                    }

                }

            }

        }

    }

    Component {
        id: experimentalPage

        Kirigami.Page {
            ColumnLayout {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                FormCard.FormCard {
                    EeSwitch {
                        id: showNativePluginUi

                        label: i18n("Native Plugin Window")
                        subtitle: i18n("Allows The Native Plugin Window to be Shown/Hidden")
                        isChecked: DB.Manager.main.showNativePluginUi
                        onCheckedChanged: {
                            if (isChecked !== DB.Manager.main.showNativePluginUi)
                                DB.Manager.main.showNativePluginUi = isChecked;

                        }
                    }

                    EeSpinBox {
                        id: lv2uiUpdateFrequency

                        label: i18n("Update Frequency")
                        subtitle: i18n("Related to LV2 Plugins")
                        from: 1
                        to: 60
                        value: DB.Manager.main.lv2uiUpdateFrequency
                        decimals: 0
                        stepSize: 1
                        unit: "Hz"
                        enabled: DB.Manager.main.showNativePluginUi
                        onValueModified: (v) => {
                            DB.Manager.main.lv2uiUpdateFrequency = v;
                        }
                    }

                }

            }

        }

    }

    Controls.StackView {
        id: stack

        implicitWidth: stack.currentItem.implicitWidth
        implicitHeight: stack.currentItem.implicitHeight

        initialItem: Kirigami.Page {
            id: childItem

            ColumnLayout {
                anchors.fill: parent

                FormCard.FormCard {
                    FormCard.FormButtonDelegate {
                        id: serviceButton

                        icon.name: "services-symbolic"
                        text: i18n("Background Service")
                        onClicked: {
                            while (stack.depth > 1)stack.pop()
                            stack.push(servicePage);
                            headerTitle.text = text;
                        }
                    }

                    FormCard.FormButtonDelegate {
                        id: audioButton

                        icon.name: "folder-sound-symbolic"
                        text: i18n("Audio")
                        onClicked: {
                            while (stack.depth > 1)stack.pop()
                            stack.push(audioPage);
                            headerTitle.text = text;
                        }
                    }

                    FormCard.FormButtonDelegate {
                        id: spectrumButton

                        icon.name: "folder-chart-symbolic"
                        text: i18n("Spectrum Analyzer")
                        onClicked: {
                            while (stack.depth > 1)stack.pop()
                            stack.push(spectrumPage);
                            headerTitle.text = text;
                        }
                    }

                    FormCard.FormButtonDelegate {
                        id: experimentalButton

                        icon.name: "emblem-warning"
                        text: i18n("Experimental Features")
                        onClicked: {
                            while (stack.depth > 1)stack.pop()
                            stack.push(experimentalPage);
                            headerTitle.text = text;
                        }
                    }

                }

            }

        }

    }

    header: RowLayout {
        Controls.ToolButton {
            id: headerBackButton

            icon.name: "draw-arrow-back"
            visible: stack.depth !== 1
            onClicked: {
                while (stack.depth > 1)stack.pop()
                headerTitle.text = i18n("Preferences");
            }
        }

        Kirigami.Icon {
            visible: stack.depth === 1
            source: "gtk-preferences-symbolic"
        }

        Kirigami.Heading {
            id: headerTitle

            Layout.fillWidth: true
            text: i18n("Preferences")
        }

    }

}
