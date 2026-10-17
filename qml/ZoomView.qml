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

// The photo on the stage: fit or zoom (wheel, pinch, keys) around the
// pointer, pan by dragging, swipe/horizontal scroll to change photo.
Item {
    id: view

    property string path
    // Pixel size after the Exif orientation (from tools.info)
    property int naturalWidth: 0
    property int naturalHeight: 0
    property bool animated: false
    // Extra rotation not saved yet, in quarter turns clockwise
    property int quarterTurns: 0
    property bool fitMode: true
    // 1.0 = one photo pixel per screen pixel
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property int reloadToken: 0
    // Swipes and pans are off while cropping
    property bool interactive: true

    readonly property real dpr: Screen.devicePixelRatio
    readonly property bool turned: quarterTurns % 2 !== 0
    readonly property real boxWidth: turned ? naturalHeight : naturalWidth
    readonly property real boxHeight: turned ? naturalWidth : naturalHeight
    readonly property real fitZoom: boxWidth > 0 && boxHeight > 0
        ? Math.min(1, width * dpr / boxWidth, height * dpr / boxHeight) : 1
    readonly property real scaleFactor: fitMode ? fitZoom : zoom
    readonly property real contentWidth: boxWidth * scaleFactor / dpr
    readonly property real contentHeight: boxHeight * scaleFactor / dpr
    readonly property bool zoomedIn: contentWidth > width + 1 || contentHeight > height + 1
    // Where the photo is drawn, in this item's coordinates (used by the crop frame)
    readonly property rect imageRect: Qt.rect(width / 2 + panX - contentWidth / 2,
                                             height / 2 + panY - contentHeight / 2,
                                             contentWidth, contentHeight)
    readonly property int status: loader.item ? loader.item.status : Image.Null
    readonly property real minZoom: Math.min(fitZoom, 0.05)
    readonly property real maxZoom: 16

    signal swipedLeft()
    signal swipedRight()
    signal tapped()
    signal doubleTapped()

    function clampPan() {
        const mx = Math.max(0, (contentWidth - width) / 2)
        const my = Math.max(0, (contentHeight - height) / 2)
        panX = Math.max(-mx, Math.min(mx, panX))
        panY = Math.max(-my, Math.min(my, panY))
    }

    function fit() {
        fitMode = true
        panX = 0
        panY = 0
    }

    function zoomAt(newZoom, px, py) {
        const old = scaleFactor
        newZoom = Math.max(minZoom, Math.min(maxZoom, newZoom))
        if (newZoom <= fitZoom + 0.0001 && old >= fitZoom - 0.0001 && newZoom < old) {
            fit()
            return
        }
        const f = newZoom / old
        const dx = px - (width / 2 + panX)
        const dy = py - (height / 2 + panY)
        fitMode = false
        zoom = newZoom
        panX = px - dx * f - width / 2
        panY = py - dy * f - height / 2
        clampPan()
    }

    function zoomBy(factor) {
        zoomAt(scaleFactor * factor, width / 2, height / 2)
    }

    function actualSize() {
        zoomAt(1, width / 2, height / 2)
    }

    function reset() {
        quarterTurns = 0
        fit()
    }

    clip: true
    onWidthChanged: clampPan()
    onHeightChanged: clampPan()
    onQuarterTurnsChanged: fit()

    Loader {
        id: loader
        // Rotation happens around the centre, so the box stays centred
        width: view.naturalWidth > 0 ? (view.naturalWidth * view.scaleFactor / view.dpr) : view.width
        height: view.naturalHeight > 0 ? (view.naturalHeight * view.scaleFactor / view.dpr) : view.height
        x: view.width / 2 + view.panX - width / 2
        y: view.height / 2 + view.panY - height / 2
        rotation: view.quarterTurns * 90
        sourceComponent: view.animated ? animatedComponent : stillComponent

        Behavior on rotation { RotationAnimation { duration: LingmoUI.Units.longDuration; easing.type: Easing.OutCubic } }
    }

    Component {
        id: stillComponent
        Image {
            source: view.path.length > 0 ? tools.urlFor(view.path) + (view.reloadToken ? "?r=" + view.reloadToken : "") : ""
            asynchronous: true
            autoTransform: true
            cache: false
            smooth: true
            mipmap: view.scaleFactor < 0.5
            fillMode: Image.Stretch
            // Vector images render sharp; giant photos stay under the GPU texture limit
            sourceSize: /\.svgz?$/i.test(view.path) ? Qt.size(2048, 2048)
                      : Math.max(view.naturalWidth, view.naturalHeight) > 8192 ? Qt.size(8192, 8192)
                      : undefined
        }
    }

    Component {
        id: animatedComponent
        AnimatedImage {
            source: view.path.length > 0 ? tools.urlFor(view.path) + (view.reloadToken ? "?r=" + view.reloadToken : "") : ""
            asynchronous: true
            cache: false
            smooth: true
            playing: true
            fillMode: Image.Stretch
        }
    }

    // Loading / failure
    LingmoUI.Spinner {
        anchors.centerIn: parent
        size: LingmoUI.Units.iconSizes.medium
        running: view.status === Image.Loading
        visible: running && spinnerDelay.ready
        Timer {
            id: spinnerDelay
            property bool ready: false
            interval: 300
            running: view.status === Image.Loading
            onRunningChanged: ready = false
            onTriggered: ready = true
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: LingmoUI.Units.smallSpacing
        visible: view.status === Image.Error

        LingmoUI.IconItem {
            anchors.horizontalCenter: parent.horizontalCenter
            width: LingmoUI.Units.iconSizes.huge
            height: width
            source: "image-x-generic"
            opacity: 0.6
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("This image can't be opened")
            font.pointSize: LingmoUI.Units.fontSubheading
            color: Qt.rgba(1, 1, 1, 0.85)
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("The file may be damaged, or its format isn't supported.")
            font.pointSize: LingmoUI.Units.fontBody
            color: Qt.rgba(1, 1, 1, 0.6)
        }
    }

    // ---- Input
    property real swipeAccumulator: 0

    WheelHandler {
        id: wheel
        enabled: view.interactive
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const dx = event.angleDelta.x
            const dy = event.angleDelta.y
            // Horizontal scroll (touchpad swipe, tilt wheel) changes photo when not zoomed
            if (Math.abs(dx) > Math.abs(dy) && !view.zoomedIn) {
                view.swipeAccumulator += dx
                if (view.swipeAccumulator <= -240) {
                    view.swipeAccumulator = 0
                    view.swipedLeft()
                } else if (view.swipeAccumulator >= 240) {
                    view.swipeAccumulator = 0
                    view.swipedRight()
                }
                swipeReset.restart()
                return
            }
            if (dy === 0)
                return
            view.zoomAt(view.scaleFactor * Math.pow(1.0015, dy), wheel.point.position.x, wheel.point.position.y)
        }
    }

    Timer {
        id: swipeReset
        interval: 300
        onTriggered: view.swipeAccumulator = 0
    }

    PinchHandler {
        id: pinch
        target: null
        enabled: view.interactive
        property real lastScale: 1
        onActiveChanged: lastScale = 1
        onActiveScaleChanged: {
            const factor = activeScale / lastScale
            lastScale = activeScale
            view.zoomAt(view.scaleFactor * factor, centroid.position.x, centroid.position.y)
        }
    }

    DragHandler {
        id: drag
        target: null
        enabled: view.interactive
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        property real lastX: 0
        property real lastY: 0
        cursorShape: active && view.zoomedIn ? Qt.ClosedHandCursor : undefined
        onActiveChanged: {
            if (active) {
                lastX = 0
                lastY = 0
            } else if (!view.zoomedIn) {
                // Swipe with a touch screen or by dragging the mouse
                if (translation.x < -80)
                    view.swipedLeft()
                else if (translation.x > 80)
                    view.swipedRight()
            }
        }
        onTranslationChanged: {
            if (!view.zoomedIn)
                return
            view.panX += translation.x - lastX
            view.panY += translation.y - lastY
            lastX = translation.x
            lastY = translation.y
            view.clampPan()
        }
    }

    TapHandler {
        enabled: view.interactive
        onTapped: (eventPoint, button) => {
            if (tapCount === 1)
                tapDelay.restart()
        }
        onDoubleTapped: {
            tapDelay.stop()
            view.doubleTapped()
        }
    }

    // A single tap waits for the double-tap interval, so it doesn't toggle twice
    Timer {
        id: tapDelay
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: view.tapped()
    }
}
