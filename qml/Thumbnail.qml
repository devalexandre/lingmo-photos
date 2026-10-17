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
import QtQuick.Window
import LingmoUI.CompatibleModule 3.0 as LingmoUI
import LingmoUI.GraphicalEffects as GE

// One square photo in the grid: cropped thumbnail, selection check, favourite heart
Item {
    id: control

    property string path
    property bool selected: false
    property bool favorite: false
    property bool selectionMode: false
    readonly property bool hovered: mouse.containsMouse || checkMouse.containsMouse || heartMouse.containsMouse

    signal activated(int modifiers)
    signal checkToggled()
    signal favoriteToggled()

    Accessible.role: Accessible.Graphic
    Accessible.name: path.substring(path.lastIndexOf("/") + 1)
    Accessible.selected: selected

    Item {
        id: frame
        anchors.fill: parent
        anchors.margins: control.selected ? 4 : 0
        scale: mouse.pressed ? 0.97 : 1.0

        Behavior on anchors.margins { NumberAnimation { duration: LingmoUI.Units.shortDuration; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: LingmoUI.Units.veryShortDuration } }

        layer.enabled: true
        layer.effect: GE.OpacityMask {
            maskSource: Rectangle {
                width: frame.width
                height: frame.height
                radius: LingmoUI.Theme.smallRadius
            }
        }

        Rectangle {
            anchors.fill: parent
            color: LingmoUI.Theme.alternateBackgroundColor
        }

        Image {
            id: image
            anchors.fill: parent
            source: control.path.length > 0 ? rootWindow.thumbUrl(control.path) : ""
            sourceSize.width: Math.round(control.width * Screen.devicePixelRatio)
            sourceSize.height: Math.round(control.height * Screen.devicePixelRatio)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.shortDuration } }
        }

        LingmoUI.IconItem {
            anchors.centerIn: parent
            width: LingmoUI.Units.iconSizes.medium
            height: width
            visible: image.status === Image.Error
            source: "image-x-generic"
        }

        // Hover shade, so the check and the heart read on light photos too
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: LingmoUI.Theme.overlayScrimColor }
                GradientStop { position: 0.35; color: "transparent" }
            }
            opacity: control.hovered || control.selectionMode ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.shortDuration } }
        }
    }

    // Selected: accent ring around the shrunk photo
    Rectangle {
        anchors.fill: parent
        radius: LingmoUI.Theme.smallRadius + 2
        color: "transparent"
        border.width: 2
        border.color: LingmoUI.Theme.highlightColor
        opacity: control.selected ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.shortDuration } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => control.activated(mouse.modifiers)
    }

    // Selection circle (top left)
    Rectangle {
        id: check
        x: LingmoUI.Units.smallSpacing + frame.anchors.margins
        y: x
        width: LingmoUI.Units.iconSizes.smallMedium
        height: width
        radius: width / 2
        visible: control.hovered || control.selectionMode || control.selected
        color: control.selected ? LingmoUI.Theme.highlightColor : Qt.rgba(0, 0, 0, 0.25)
        border.width: control.selected ? 0 : 1.5
        border.color: Qt.rgba(1, 1, 1, 0.9)

        Image {
            anchors.centerIn: parent
            width: LingmoUI.Units.iconSizes.small
            height: width
            source: "qrc:/images/check.svg"
            sourceSize: Qt.size(32, 32)
            visible: control.selected
        }

        MouseArea {
            id: checkMouse
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.checkToggled()
        }

        ToolTip.visible: checkMouse.containsMouse
        ToolTip.delay: 600
        ToolTip.text: control.selected ? qsTr("Deselect") : qsTr("Select")
    }

    // Favourite heart (top right): always shown when set, on hover otherwise
    Item {
        x: parent.width - width - LingmoUI.Units.smallSpacing - frame.anchors.margins
        y: LingmoUI.Units.smallSpacing + frame.anchors.margins
        width: LingmoUI.Units.iconSizes.smallMedium
        height: width
        visible: control.favorite || control.hovered

        Image {
            anchors.centerIn: parent
            width: 18
            height: 18
            source: control.favorite ? "qrc:/images/favorite-on.svg" : "qrc:/images/favorites.svg"
            sourceSize: Qt.size(36, 36)
            opacity: control.favorite || heartMouse.containsMouse ? 1 : 0.8
            scale: heartMouse.pressed ? 0.85 : 1
            Behavior on scale { NumberAnimation { duration: LingmoUI.Units.veryShortDuration } }
        }

        MouseArea {
            id: heartMouse
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.favoriteToggled()
        }

        ToolTip.visible: heartMouse.containsMouse
        ToolTip.delay: 600
        ToolTip.text: control.favorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
    }
}
