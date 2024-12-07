import "Common.js" as Common
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import ee.database as DB
import org.kde.kirigami as Kirigami

Kirigami.OverlaySheet {
    id: control

    required property var streamDB

    parent: applicationWindow().overlay
    closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
    focus: true
    showCloseButton: false
    y: appWindow.header.height + Kirigami.Units.gridUnit
    title: i18n("Effects Blocklist")

    ColumnLayout {
        id: columnLayout

        Layout.preferredWidth: Kirigami.Units.gridUnit * 30
        implicitHeight: control.parent.height - 2 * (control.header.height + control.footer.height) - control.y

        Kirigami.ActionTextField {
            id: newBlockedApp

            Layout.fillWidth: true
            placeholderText: i18n("Application Node Name")
            // based on https://github.com/KDE/kirigami/blob/master/src/controls/SearchField.qml
            leftPadding: {
                if (effectiveHorizontalAlignment === TextInput.AlignRight)
                    return _rightActionsRow.width + Kirigami.Units.smallSpacing;
                else
                    return creationIcon.width + Kirigami.Units.smallSpacing * 3;
            }
            rightPadding: {
                if (effectiveHorizontalAlignment === TextInput.AlignRight)
                    return creationIcon.width + Kirigami.Units.smallSpacing * 3;
                else
                    return _rightActionsRow.width + Kirigami.Units.smallSpacing;
            }
            rightActions: [
                Kirigami.Action {
                    text: i18n("Add to Excluded Applications")
                    icon.name: "list-add-symbolic"
                    onTriggered: {
                        const name = newBlockedApp.text;
                        if (!Common.isEmpty(name)) {
                            if (!streamDB.blocklist.includes(name)) {
                                streamDB.blocklist.push(name);
                                newBlockedApp.text = "";
                            }
                        }
                    }
                }
            ]

            Kirigami.Icon {
                id: creationIcon

                LayoutMirroring.enabled: newBlockedApp.effectiveHorizontalAlignment === TextInput.AlignRight
                anchors.left: newBlockedApp.left
                anchors.leftMargin: Kirigami.Units.smallSpacing * 2
                anchors.verticalCenter: newBlockedApp.verticalCenter
                anchors.verticalCenterOffset: Math.round((newBlockedApp.topPadding - newBlockedApp.bottomPadding) / 2)
                implicitHeight: Kirigami.Units.iconSizes.sizeForLabels
                implicitWidth: Kirigami.Units.iconSizes.sizeForLabels
                color: newBlockedApp.placeholderTextColor
                source: "bookmarks-symbolic"
            }

            validator: RegularExpressionValidator {
                regularExpression: /[^\\/]{100}$/ //less than 100 characters and no / or \
            }

        }

        ListView {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            reuseItems: true
            model: streamDB.blocklist

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                visible: listView.count === 0
                text: i18n("Empty")
            }

            delegate: Controls.ItemDelegate {
                id: listItemDelegate

                readonly property string name: {
                    if (streamDB.blocklist.length > 0)
                        return streamDB.blocklist[index];

                    return "";
                }
                property bool selected: listItemDelegate.highlighted || listItemDelegate.down
                property color color: selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor

                hoverEnabled: true
                width: listView.width

                contentItem: RowLayout {
                    Controls.Label {
                        text: name
                    }

                    Kirigami.ActionToolBar {
                        alignment: Qt.AlignRight
                        actions: [
                            Kirigami.Action {
                                text: i18n("Delete this Preset")
                                icon.name: "delete"
                                displayHint: Kirigami.DisplayHint.AlwaysHide
                                onTriggered: {
                                    const targetIndex = streamDB.blocklist.indexOf(name);
                                    if (targetIndex > -1)
                                        streamDB.blocklist.splice(targetIndex, 1);

                                }
                            }
                        ]
                    }

                }

            }

        }

    }

    footer: Kirigami.ActionToolBar {
        alignment: Qt.AlignRight
        position: Controls.ToolBar.Footer
        actions: [
            Kirigami.Action {
                text: i18n("Show Excluded Applications in the Streams Section")
                icon.name: "applications-all-symbolic"
                displayHint: Kirigami.DisplayHint.KeepVisible
                checkable: true
                checked: streamDB.showBlocklistedApps
                onTriggered: {
                    if (checked !== streamDB.showBlocklistedApps)
                        streamDB.showBlocklistedApps = checked;

                }
            }
        ]
    }

}
