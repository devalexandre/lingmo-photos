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
import LingmoUI.CompatibleModule 3.0 as LingmoUI

// Crop frame over the photo: drag the corners/edges to resize, the inside to move.
Item {
    id: overlay

    // Photo area, in this item's coordinates
    property rect bounds: Qt.rect(0, 0, width, height)
    // width / height of the selection; 0 = free
    property real aspect: 0
    property real selX: 0
    property real selY: 0
    property real selW: 0
    property real selH: 0
    readonly property real minSize: 24
    // Selection relative to the photo (0..1), for ImageTools.crop()
    readonly property rect normalized: bounds.width > 0 && bounds.height > 0
        ? Qt.rect((selX - bounds.x) / bounds.width, (selY - bounds.y) / bounds.height,
                  selW / bounds.width, selH / bounds.height)
        : Qt.rect(0, 0, 1, 1)

    function reset() {
        selX = bounds.x
        selY = bounds.y
        selW = bounds.width
        selH = bounds.height
        applyAspect()
    }

    // Largest selection with the aspect, centred on the current one
    function applyAspect() {
        if (aspect <= 0)
            return
        const cx = selX + selW / 2
        const cy = selY + selH / 2
        let w = bounds.width
        let h = w / aspect
        if (h > bounds.height) {
            h = bounds.height
            w = h * aspect
        }
        selW = w
        selH = h
        selX = Math.max(bounds.x, Math.min(bounds.x + bounds.width - w, cx - w / 2))
        selY = Math.max(bounds.y, Math.min(bounds.y + bounds.height - h, cy - h / 2))
    }

    onAspectChanged: applyAspect()
    onBoundsChanged: reset()

    // Dimmed photo outside the selection
    readonly property color shade: Qt.rgba(0, 0, 0, 0.55)
    Rectangle { x: 0; y: 0; width: parent.width; height: overlay.selY; color: overlay.shade }
    Rectangle { x: 0; y: overlay.selY + overlay.selH; width: parent.width; height: parent.height - y; color: overlay.shade }
    Rectangle { x: 0; y: overlay.selY; width: overlay.selX; height: overlay.selH; color: overlay.shade }
    Rectangle { x: overlay.selX + overlay.selW; y: overlay.selY; width: parent.width - x; height: overlay.selH; color: overlay.shade }

    Item {
        id: frame
        x: overlay.selX
        y: overlay.selY
        width: overlay.selW
        height: overlay.selH

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1.5
            border.color: Qt.rgba(1, 1, 1, 0.95)
        }

        // Rule of thirds while dragging
        Repeater {
            model: 2
            Rectangle {
                required property int index
                x: frame.width * (index + 1) / 3
                width: 1
                height: frame.height
                color: Qt.rgba(1, 1, 1, 0.45)
                visible: moveArea.pressed || overlay.resizing
            }
        }
        Repeater {
            model: 2
            Rectangle {
                required property int index
                y: frame.height * (index + 1) / 3
                height: 1
                width: frame.width
                color: Qt.rgba(1, 1, 1, 0.45)
                visible: moveArea.pressed || overlay.resizing
            }
        }

        MouseArea {
            id: moveArea
            anchors.fill: parent
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point start
            property real startX
            property real startY
            onPressed: mouse => {
                start = mapToItem(overlay, mouse.x, mouse.y)
                startX = overlay.selX
                startY = overlay.selY
            }
            onPositionChanged: mouse => {
                const p = mapToItem(overlay, mouse.x, mouse.y)
                const b = overlay.bounds
                overlay.selX = Math.max(b.x, Math.min(b.x + b.width - overlay.selW, startX + p.x - start.x))
                overlay.selY = Math.max(b.y, Math.min(b.y + b.height - overlay.selH, startY + p.y - start.y))
            }
        }
    }

    property bool resizing: false

    // Handles: hx/hy = -1 left/top, 1 right/bottom, 0 middle
    Repeater {
        model: [
            { hx: -1, hy: -1 }, { hx: 1, hy: -1 }, { hx: -1, hy: 1 }, { hx: 1, hy: 1 },
            { hx: 0, hy: -1 }, { hx: 0, hy: 1 }, { hx: -1, hy: 0 }, { hx: 1, hy: 0 }
        ]

        Item {
            id: handle
            required property var modelData
            readonly property bool corner: modelData.hx !== 0 && modelData.hy !== 0
            readonly property int size: 28

            visible: corner || overlay.aspect <= 0
            width: size
            height: size
            x: overlay.selX + (modelData.hx + 1) / 2 * overlay.selW - size / 2
            y: overlay.selY + (modelData.hy + 1) / 2 * overlay.selH - size / 2

            // Corner brackets / edge bars
            Rectangle {
                anchors.centerIn: parent
                width: handle.modelData.hx === 0 ? 22 : 6
                height: handle.modelData.hy === 0 ? 22 : 6
                radius: 3
                color: "white"
                visible: !handle.corner
            }
            // L-shaped bracket inside the selection, its corner on the selection corner
            Rectangle {
                visible: handle.corner
                x: handle.modelData.hx < 0 ? handle.size / 2 - 1 : handle.size / 2 - 21
                y: handle.modelData.hy < 0 ? handle.size / 2 - 1 : handle.size / 2 - 3
                width: 22
                height: 4
                radius: 1
                color: "white"
            }
            Rectangle {
                visible: handle.corner
                x: handle.modelData.hx < 0 ? handle.size / 2 - 1 : handle.size / 2 - 3
                y: handle.modelData.hy < 0 ? handle.size / 2 - 1 : handle.size / 2 - 21
                width: 4
                height: 22
                radius: 1
                color: "white"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: {
                    const d = handle.modelData
                    if (d.hx === 0)
                        return Qt.SizeVerCursor
                    if (d.hy === 0)
                        return Qt.SizeHorCursor
                    return d.hx === d.hy ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                }
                property point start
                property rect startRect
                onPressed: mouse => {
                    start = mapToItem(overlay, mouse.x, mouse.y)
                    startRect = Qt.rect(overlay.selX, overlay.selY, overlay.selW, overlay.selH)
                    overlay.resizing = true
                }
                onReleased: overlay.resizing = false
                onCanceled: overlay.resizing = false
                onPositionChanged: mouse => {
                    const p = mapToItem(overlay, mouse.x, mouse.y)
                    const d = handle.modelData
                    const b = overlay.bounds
                    const s = startRect
                    const m = overlay.minSize
                    let left = s.x, top = s.y, right = s.x + s.width, bottom = s.y + s.height
                    if (d.hx < 0)
                        left = Math.max(b.x, Math.min(right - m, s.x + p.x - start.x))
                    else if (d.hx > 0)
                        right = Math.min(b.x + b.width, Math.max(left + m, s.x + s.width + p.x - start.x))
                    if (d.hy < 0)
                        top = Math.max(b.y, Math.min(bottom - m, s.y + p.y - start.y))
                    else if (d.hy > 0)
                        bottom = Math.min(b.y + b.height, Math.max(top + m, s.y + s.height + p.y - start.y))

                    if (overlay.aspect > 0) {
                        // Width leads; the height follows from the aspect, from the fixed corner
                        let w = right - left
                        let h = w / overlay.aspect
                        const maxH = d.hy < 0 ? bottom - b.y : b.y + b.height - top
                        if (h > maxH) {
                            h = maxH
                            w = h * overlay.aspect
                            if (d.hx < 0)
                                left = right - w
                            else
                                right = left + w
                        }
                        if (d.hy < 0)
                            top = bottom - h
                        else
                            bottom = top + h
                    }
                    overlay.selX = left
                    overlay.selY = top
                    overlay.selW = right - left
                    overlay.selH = bottom - top
                }
            }
        }
    }
}
