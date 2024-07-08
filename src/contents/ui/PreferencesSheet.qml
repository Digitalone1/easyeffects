import EEdb
import QtQml
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard 1.0 as FormCard

Kirigami.OverlaySheet {
    id: preferencesSheet

    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: appWindow.height * 0.7
    title: i18n("Preferences")
    onVisibleChanged: {
        if (!preferencesSheet.visible)
            while (stack.depth > 1)stack.pop();

    }

    Component {
        id: mainPage

        Kirigami.Page {
            FormCard.FormCard {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

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
                }

                FormCard.FormButtonDelegate {
                    id: experimentalButton

                    icon.name: "emblem-warning"
                    text: i18n("Experimental Features")
                }

            }

        }

    }

    Component {
        id: servicePage

        Kirigami.Page {
            FormCard.FormCard {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                EESwitch {
                    id: launchServiceOnLogin

                    label: i18n("Launch Service at System Startup")
                    // isChecked: EEdb.addRandom
                    onCheckedChanged: {
                    }
                }

                EESwitch {
                    id: showTrayIcon

                    label: i18n("Show the Tray Icon")
                    isChecked: EEdb.showTrayIcon
                    onCheckedChanged: {
                        if (isChecked !== EEdb.showTrayIcon)
                            EEdb.showTrayIcon = isChecked;

                    }
                }

            }

        }

    }

    Component {
        id: audioPage

        Kirigami.Page {
            FormCard.FormCard {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.smallSpacing
                    right: parent.right
                    rightMargin: Kirigami.Units.smallSpacing
                }

                EESwitch {
                    id: processAllOutputs

                    label: i18n("Process All Output Streams")
                    isChecked: EEdb.processAllOutputs
                    onCheckedChanged: {
                        if (isChecked !== EEdb.processAllOutputs)
                            EEdb.processAllOutputs = isChecked;

                    }
                }

                EESwitch {
                    id: processAllInputs

                    label: i18n("Process All Input Streams")
                    isChecked: EEdb.processAllInputs
                    onCheckedChanged: {
                        if (isChecked !== EEdb.processAllInputs)
                            EEdb.processAllInputs = isChecked;

                    }
                }

                EESwitch {
                    id: excludeMonitorStreams

                    label: i18n("Ignore Streams from Monitor of Devices")
                    isChecked: EEdb.excludeMonitorStreams
                    onCheckedChanged: {
                        if (isChecked !== EEdb.excludeMonitorStreams)
                            EEdb.excludeMonitorStreams = isChecked;

                    }
                }

                EESwitch {
                    id: useCubicVolumes

                    label: i18n("Use Cubic Volume")
                    isChecked: EEdb.useCubicVolumes
                    onCheckedChanged: {
                        if (isChecked !== EEdb.useCubicVolumes)
                            EEdb.useCubicVolumes = isChecked;

                    }
                }

                EESwitch {
                    id: inactivityTimerEnable

                    label: i18n("Enable the Inactivity Timeout")
                    isChecked: EEdb.inactivityTimerEnable
                    onCheckedChanged: {
                        if (isChecked !== EEdb.inactivityTimerEnable)
                            EEdb.inactivityTimerEnable = isChecked;

                    }
                }

                EESpinBox {
                    id: inactivityTimeout

                    label: i18n("Inactivity Timeout")
                    subtitle: i18n("Input Pipeline")
                    from: 1
                    to: 3600
                    value: EEdb.inactivityTimeout
                    decimals: 0
                    stepSize: 1
                    unit: "s"
                    enabled: EEdb.inactivityTimerEnable
                    onValueModified: (v) => {
                        EEdb.inactivityTimeout = v;
                    }
                }

            }

        }

    }

    Controls.StackView {
        id: stack

        initialItem: mainPage

        anchors {
            left: parent.left
            leftMargin: Kirigami.Units.smallSpacing
            right: parent.right
            rightMargin: Kirigami.Units.smallSpacing
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
