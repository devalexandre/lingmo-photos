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

// Thumbnail grid grouped by month. The model has one row per month header
// or per line of thumbnails (library.rows), so only what's on screen exists.
ListView {
    id: grid

    property int minCell: 170
    readonly property int spacingPx: 4
    readonly property int columns: Math.max(1, Math.floor((width + spacingPx) / (minCell + spacingPx)))
    readonly property real cell: (width - spacingPx * (columns - 1)) / columns
    // Anchor of Shift+click ranges
    property int anchorIndex: -1

    signal openPhoto(int index)
    signal deletePressed()
    signal zoomRequested(int delta)

    function positionAtPhoto(index) {
        const row = library.rowForPhoto(index)
        if (row >= 0)
            positionViewAtIndex(row, ListView.Contain)
    }

    function activate(index, modifiers) {
        forceActiveFocus()
        if (modifiers & Qt.ShiftModifier && anchorIndex >= 0) {
            library.selectRange(anchorIndex, index)
        } else if (modifiers & Qt.ControlModifier || library.selectedCount > 0) {
            library.toggleSelected(index)
            anchorIndex = index
        } else {
            anchorIndex = index
            grid.openPhoto(index)
        }
    }

    model: library.rows
    clip: true
    spacing: grid.spacingPx
    boundsBehavior: Flickable.StopAtBounds
    bottomMargin: LingmoUI.Units.largeSpacing * 2
    reuseItems: true
    cacheBuffer: 800
    keyNavigationEnabled: false
    focus: true

    onColumnsChanged: library.columns = columns
    Component.onCompleted: library.columns = columns

    ScrollBar.vertical: ScrollBar {}

    LingmoUI.WheelHandler {
        target: grid
    }

    // Ctrl + wheel changes the thumbnail size
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: event => grid.zoomRequested(event.angleDelta.y > 0 ? 1 : -1)
    }

    Keys.onDeletePressed: grid.deletePressed()
    Keys.onPressed: event => {
        if (event.modifiers & Qt.ControlModifier && (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal)) {
            grid.zoomRequested(1)
            event.accepted = true
        } else if (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_Minus) {
            grid.zoomRequested(-1)
            event.accepted = true
        }
    }

    delegate: Item {
        id: rowItem

        required property bool header
        required property string title
        required property int groupCount
        required property int first
        required property int count

        width: grid.width
        height: header ? headerRow.implicitHeight + LingmoUI.Units.largeSpacing * 1.5 : grid.cell

        RowLayout {
            id: headerRow
            visible: rowItem.header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: LingmoUI.Units.smallSpacing
            spacing: LingmoUI.Units.smallSpacing

            Label {
                text: rowItem.title
                font.pointSize: LingmoUI.Units.fontSubheading
                font.bold: true
                color: LingmoUI.Theme.textColor
            }
            Label {
                text: rootWindow.photoCount(rowItem.groupCount)
                font.pointSize: LingmoUI.Units.fontCaption
                color: LingmoUI.Theme.secondaryTextColor
                Layout.alignment: Qt.AlignBaseline
            }
            Item { Layout.fillWidth: true }
        }

        Row {
            visible: !rowItem.header
            spacing: grid.spacingPx

            Repeater {
                model: rowItem.header ? 0 : rowItem.count

                Thumbnail {
                    required property int index
                    readonly property int photoIndex: rowItem.first + index

                    width: grid.cell
                    height: grid.cell
                    path: (library.revision, library.path(photoIndex))
                    selected: library.revision >= 0 && library.isSelected(photoIndex)
                    favorite: library.revision >= 0 && library.isFavorite(path)
                    selectionMode: library.selectedCount > 0
                    onActivated: modifiers => grid.activate(photoIndex, modifiers)
                    onCheckToggled: {
                        library.toggleSelected(photoIndex)
                        grid.anchorIndex = photoIndex
                    }
                    onFavoriteToggled: library.setFavorite(path, !favorite)
                }
            }
        }
    }
}
