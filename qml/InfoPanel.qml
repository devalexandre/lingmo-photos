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

// Details of the current photo: size, dates, camera, location
LingmoUI.Card {
    id: panel

    property var info: ({})
    property bool favorite: false

    signal closeRequested()
    signal showInFolder()

    elevation: 2
    radius: LingmoUI.Theme.bigRadius

    component Field: ColumnLayout {
        property string label
        property string value
        Layout.fillWidth: true
        visible: value.length > 0
        spacing: 1

        Label {
            Layout.fillWidth: true
            text: parent.label
            font.pointSize: LingmoUI.Units.fontCaption
            color: LingmoUI.Theme.secondaryTextColor
        }
        Label {
            Layout.fillWidth: true
            text: parent.value
            wrapMode: Text.Wrap
            font.pointSize: LingmoUI.Units.fontBody
            color: LingmoUI.Theme.textColor
        }
    }

    component SectionTitle: Label {
        Layout.fillWidth: true
        Layout.topMargin: LingmoUI.Units.smallSpacing
        font.pointSize: LingmoUI.Units.fontCaption
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: LingmoUI.Theme.secondaryTextColor
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: LingmoUI.Units.largeSpacing
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: flick.width
            spacing: LingmoUI.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Info")
                    font.pointSize: LingmoUI.Units.fontHeading
                    font.bold: true
                }
                LingmoUI.IconButton {
                    iconName: "window-close"
                    tipText: qsTr("Close")
                    onClicked: panel.closeRequested()
                }
            }

            Label {
                Layout.fillWidth: true
                text: panel.info.name || ""
                wrapMode: Text.WrapAnywhere
                font.pointSize: LingmoUI.Units.fontSubheading
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: LingmoUI.Units.smallSpacing
                LingmoUI.Badge { text: panel.info.format || ""; subtle: true }
                LingmoUI.Badge { text: panel.info.animated ? qsTr("Animated") : ""; subtle: true; tone: LingmoUI.Theme.purpleColor }
                LingmoUI.Badge { text: panel.favorite ? qsTr("Favorite") : ""; subtle: true; tone: LingmoUI.Theme.pinkColor }
                Item { Layout.fillWidth: true }
            }

            SectionTitle { text: qsTr("Image") }
            Field { label: qsTr("Dimensions"); value: panel.info.dimensions || "" }
            Field { label: qsTr("Resolution"); value: panel.info.megapixels || "" }
            Field { label: qsTr("File size"); value: panel.info.fileSize || "" }

            SectionTitle { text: qsTr("Date") }
            Field { label: qsTr("Taken"); value: panel.info.taken || "" }
            Field { label: qsTr("Modified"); value: panel.info.modified || "" }

            SectionTitle {
                text: qsTr("Camera")
                visible: (panel.info.camera || "").length > 0 || (panel.info.exposure || "").length > 0
            }
            Field { label: qsTr("Camera"); value: panel.info.camera || "" }
            Field { label: qsTr("Lens"); value: panel.info.lens || "" }
            Field { label: qsTr("Exposure"); value: panel.info.exposure || "" }

            SectionTitle { text: qsTr("Location") }
            Field { label: qsTr("Folder"); value: panel.info.folder || "" }

            LingmoUI.ActionButton {
                Layout.topMargin: LingmoUI.Units.smallSpacing
                text: qsTr("Show in Folder")
                iconName: "folder-open"
                onClicked: panel.showInFolder()
            }
        }
    }
}
