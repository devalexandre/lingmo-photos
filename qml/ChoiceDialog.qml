/*
 * Copyright (C) 2026 LingmoOS Team.
 *
 * Author:     devalexandre <alexandre@dev2learn.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LingmoUI.CompatibleModule 3.0 as LingmoUI

// Small question dialog: a title, a text and a row of choices
// ({ id, text, primary, destructive }); emits chosen(id), "" when cancelled.
Popup {
    id: dialog

    property string title
    property string text
    property var choices: []

    signal chosen(string id)

    function ask(title, text, choices) {
        dialog.title = title
        dialog.text = text
        dialog.choices = choices
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - LingmoUI.Units.largeSpacing * 4 : 460)
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    padding: LingmoUI.Units.largeSpacing * 2
    onClosed: if (!internal.answered) chosen("")
    onOpened: internal.answered = false

    QtObject {
        id: internal
        property bool answered: false
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.35)
        Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.shortDuration } }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: LingmoUI.Units.shortDuration }
        NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: LingmoUI.Units.shortDuration; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: LingmoUI.Units.veryShortDuration }
    }

    background: LingmoUI.Card {
        radius: LingmoUI.Theme.hugeRadius
        elevation: 3
    }

    contentItem: ColumnLayout {
        spacing: LingmoUI.Units.largeSpacing

        Label {
            Layout.fillWidth: true
            text: dialog.title
            wrapMode: Text.Wrap
            font.pointSize: LingmoUI.Units.fontHeading
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            text: dialog.text
            visible: text.length > 0
            wrapMode: Text.Wrap
            font.pointSize: LingmoUI.Units.fontBody
            color: LingmoUI.Theme.secondaryTextColor
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: LingmoUI.Units.smallSpacing
            spacing: LingmoUI.Units.smallSpacing

            Item { Layout.fillWidth: true }

            LingmoUI.ActionButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }

            Repeater {
                model: dialog.choices
                LingmoUI.ActionButton {
                    required property var modelData
                    text: modelData.text
                    primary: !!modelData.primary
                    destructive: !!modelData.destructive
                    onClicked: {
                        internal.answered = true
                        dialog.close()
                        dialog.chosen(modelData.id)
                    }
                }
            }
        }
    }
}
