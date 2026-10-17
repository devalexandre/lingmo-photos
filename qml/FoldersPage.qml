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
import QtQuick.Window
import LingmoUI.CompatibleModule 3.0 as LingmoUI
import LingmoUI.GraphicalEffects as GE

// Folders that contain photos, as cards with the newest photo as cover
Item {
    id: page

    readonly property int count: view.count

    signal openFolder(string path, string name)

    GridView {
        id: view

        readonly property int columns: Math.max(1, Math.floor(width / 220))
        anchors.fill: parent
        model: library.folders
        clip: true
        cellWidth: Math.floor(width / columns)
        cellHeight: cellWidth * 0.75 + 64
        boundsBehavior: Flickable.StopAtBounds
        bottomMargin: LingmoUI.Units.largeSpacing * 2
        ScrollBar.vertical: ScrollBar {}

        LingmoUI.WheelHandler { target: view }

        delegate: Item {
            id: cell

            required property string path
            required property string name
            required property string relative
            required property string cover
            required property int count

            width: view.cellWidth
            height: view.cellHeight

            LingmoUI.Card {
                id: card
                anchors.fill: parent
                anchors.margins: LingmoUI.Units.smallSpacing
                clickable: true
                liftOnHover: true
                onClicked: page.openFolder(cell.path, cell.name)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: LingmoUI.Units.smallSpacing
                    spacing: LingmoUI.Units.smallSpacing

                    Item {
                        id: coverFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        layer.enabled: true
                        layer.effect: GE.OpacityMask {
                            maskSource: Rectangle {
                                width: coverFrame.width
                                height: coverFrame.height
                                radius: LingmoUI.Theme.smallRadius
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: LingmoUI.Theme.alternateBackgroundColor
                        }

                        Image {
                            anchors.fill: parent
                            source: rootWindow.thumbUrl(cell.cover)
                            sourceSize.width: Math.round(width * Screen.devicePixelRatio)
                            sourceSize.height: Math.round(height * Screen.devicePixelRatio)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            scale: card.hovered ? 1.04 : 1.0
                            Behavior on scale { NumberAnimation { duration: LingmoUI.Units.longDuration; easing.type: Easing.OutCubic } }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: LingmoUI.Units.smallSpacing
                        Layout.rightMargin: LingmoUI.Units.smallSpacing
                        Layout.bottomMargin: LingmoUI.Units.smallSpacing / 2
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            text: cell.name
                            elide: Text.ElideRight
                            font.pointSize: LingmoUI.Units.fontBody
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            // Nested folders also show where they are
                            text: cell.relative.indexOf("/") < 0 ? rootWindow.photoCount(cell.count)
                                  : rootWindow.photoCount(cell.count) + " · " + cell.relative.substring(0, cell.relative.lastIndexOf("/"))
                            elide: Text.ElideMiddle
                            font.pointSize: LingmoUI.Units.fontCaption
                            color: LingmoUI.Theme.secondaryTextColor
                        }
                    }
                }
            }
        }
    }

    LingmoUI.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - LingmoUI.Units.largeSpacing * 4, 420)
        visible: view.count === 0
        busy: library.scanning
        iconName: "folder-pictures"
        title: library.scanning ? qsTr("Looking for photos…") : qsTr("No folders with photos")
        text: library.scanning ? "" : qsTr("Folders inside %1 that have pictures show up here.").arg(tools.prettyPath(library.rootFolder))
    }
}
