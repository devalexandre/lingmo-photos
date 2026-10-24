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

// Immersive viewer: the photo on a dark stage, controls that fade away,
// prev/next through a list (the folder, or the library grid), slideshow.
Item {
    id: viewer

    property bool active: false
    property color stageColor
    property var paths: []
    property int index: -1
    readonly property string currentPath: index >= 0 && index < paths.length ? paths[index] : ""
    readonly property string fileName: currentPath.length > 0 ? tools.fileName(currentPath) : ""
    property var info: ({})
    property bool slideshow: false
    property bool slideshowPaused: false
    property bool cropping: false
    property bool infoVisible: false
    property bool controlsVisible: true
    // Last deleted photo, so "Undo" puts it back in place
    property var lastRemoved: null
    property var openWithApps: []
    // What the question dialog is about: "rotate" or "replace"
    property string pendingQuestion: ""
    property bool wasFullScreen: false

    readonly property bool fullScreen: Window.window ? Window.window.visibility === Window.FullScreen : false
    readonly property bool rotationPending: zoomView.quarterTurns % 4 !== 0
    readonly property bool favorite: library.revision >= 0 && library.isFavorite(currentPath)
    readonly property bool pinned: cropping || rotationPending || moreMenu.visible || choiceDialog.visible
                                   || topHover.hovered || bottomHover.hovered || sideHoverL.hovered || sideHoverR.hovered
                                   || !active

    signal backRequested()
    signal fullScreenRequested()
    signal deleteRequested(string path)
    signal emptied()

    function open(list, startIndex, startSlideshow) {
        cropping = false
        paths = list
        index = Math.max(0, Math.min(startIndex, list.length - 1))
        refresh()
        if (startSlideshow)
            startSlideshowMode()
        else
            poke()
    }

    function refresh() {
        info = currentPath.length > 0 ? tools.info(currentPath) : ({})
        zoomView.reset()
    }

    function go(newIndex) {
        if (paths.length === 0 || cropping)
            return
        if (slideshow)
            newIndex = (newIndex + paths.length) % paths.length
        else
            newIndex = Math.max(0, Math.min(paths.length - 1, newIndex))
        if (newIndex === index)
            return
        if (slideshow) {
            fade.target = newIndex
            fade.restart()
        } else {
            index = newIndex
        }
    }

    function next() { go(index + 1) }
    function previous() { go(index - 1) }

    function removeCurrent() {
        if (index < 0)
            return
        lastRemoved = { path: currentPath, index: index }
        const list = paths.slice()
        list.splice(index, 1)
        paths = list
        if (list.length === 0) {
            index = -1
            emptied()
            return
        }
        const keep = Math.min(index, list.length - 1)
        index = -1 // currentPath changes even when the number stays the same
        index = keep
    }

    function restore(restored) {
        if (!lastRemoved || restored.indexOf(lastRemoved.path) < 0)
            return
        const list = paths.slice()
        const at = Math.min(lastRemoved.index, list.length)
        list.splice(at, 0, lastRemoved.path)
        paths = list
        lastRemoved = null
        if (active)
            index = at
    }

    function poke() {
        controlsVisible = true
        hideTimer.restart()
    }

    function toast(message) {
        if (Window.window)
            Window.window.showPassiveNotification(message, "short")
        noteToast(4000)
    }

    // Toasts sit at the bottom of the window: lift the zoom bar meanwhile
    property bool toastShown: false
    function noteToast(ms) {
        toastShown = true
        toastTimer.interval = ms + 500
        toastTimer.restart()
    }
    Timer {
        id: toastTimer
        onTriggered: viewer.toastShown = false
    }

    function startSlideshowMode() {
        if (paths.length === 0)
            return
        slideshow = true
        slideshowPaused = false
        infoVisible = false
        controlsVisible = false
        wasFullScreen = fullScreen
        if (!fullScreen)
            fullScreenRequested()
    }

    function stopSlideshow() {
        if (!slideshow)
            return
        slideshow = false
        if (fullScreen && !wasFullScreen)
            fullScreenRequested()
        poke()
    }

    function rotate(turns) {
        if (cropping)
            return
        zoomView.quarterTurns = (zoomView.quarterTurns + turns + 4) % 4
        poke()
    }

    function askSaveRotation() {
        pendingQuestion = "rotate"
        if (tools.canRotateLosslessly(currentPath)) {
            choiceDialog.ask(qsTr("Save the rotated photo?"),
                             qsTr("“Save Orientation” turns the photo without touching its pixels, by updating its orientation tag (no quality loss). “Rotate Pixels” saves the image again, for apps that ignore the tag."),
                             [{ id: "reencode", text: qsTr("Rotate Pixels") },
                              { id: "lossless", text: qsTr("Save Orientation"), primary: true }])
        } else if (tools.canReplace(currentPath)) {
            choiceDialog.ask(qsTr("Save the rotated photo?"),
                             qsTr("The image will be saved again with its pixels rotated. JPEG photos lose a little quality each time they're saved."),
                             [{ id: "reencode", text: qsTr("Rotate and Save"), primary: true }])
        } else {
            toast(qsTr("This file can't be changed. Crop it to save a rotated copy."))
        }
    }

    function saveRotation(lossless) {
        if (tools.rotate(currentPath, zoomView.quarterTurns, lossless)) {
            zoomView.quarterTurns = 0
            zoomView.reloadToken++
            info = tools.info(currentPath)
        } else {
            toast(tools.lastError)
        }
    }

    function startCrop() {
        if (zoomView.status !== Image.Ready)
            return
        infoVisible = false
        zoomView.fit()
        cropping = true
        aspectControl.currentIndex = 0
        cropOverlay.aspect = 0
        Qt.callLater(cropOverlay.reset)
    }

    function finishCrop(replace) {
        const saved = tools.crop(currentPath, cropOverlay.normalized, zoomView.quarterTurns, replace)
        if (saved.length === 0) {
            toast(tools.lastError)
            return
        }
        cropping = false
        if (saved === currentPath) {
            zoomView.reset()
            zoomView.reloadToken++
            info = tools.info(currentPath)
            toast(qsTr("Photo cropped"))
        } else {
            const list = paths.slice()
            list.splice(index + 1, 0, saved)
            paths = list
            index = index + 1
            toast(qsTr("Saved as “%1”").arg(tools.fileName(saved)))
        }
    }

    onCurrentPathChanged: {
        cropping = false
        refresh()
    }

    onActiveChanged: if (!active) stopSlideshow()

    // ---- Stage

    ZoomView {
        id: zoomView
        anchors.fill: parent
        anchors.topMargin: viewer.cropping ? LingmoUI.Units.windowHeaderHeight + LingmoUI.Units.largeSpacing : 0
        anchors.bottomMargin: viewer.cropping ? cropBar.height + LingmoUI.Units.largeSpacing * 3 : 0
        anchors.leftMargin: viewer.cropping ? LingmoUI.Units.largeSpacing * 2 : 0
        anchors.rightMargin: viewer.infoVisible ? infoPanel.width + LingmoUI.Units.largeSpacing * 2
                           : viewer.cropping ? LingmoUI.Units.largeSpacing * 2 : 0
        path: viewer.currentPath
        naturalWidth: viewer.info.width || 0
        naturalHeight: viewer.info.height || 0
        animated: !!viewer.info.animated
        interactive: !viewer.cropping

        Behavior on anchors.rightMargin { NumberAnimation { duration: LingmoUI.Units.longDuration; easing.type: Easing.OutCubic } }

        onSwipedLeft: viewer.next()
        onSwipedRight: viewer.previous()
        onTapped: {
            if (viewer.slideshow)
                viewer.stopSlideshow()
            else if (viewer.controlsVisible)
                viewer.controlsVisible = false
            else
                viewer.poke()
        }
        onDoubleTapped: viewer.fullScreenRequested()

        CropOverlay {
            id: cropOverlay
            anchors.fill: parent
            visible: viewer.cropping
            enabled: viewer.cropping
            bounds: zoomView.imageRect
        }
    }

    // Cross-fade between slides
    SequentialAnimation {
        id: fade
        property int target: 0
        NumberAnimation { target: zoomView; property: "opacity"; to: 0; duration: LingmoUI.Units.longDuration }
        ScriptAction { script: viewer.index = fade.target }
        PauseAnimation { duration: 60 }
        NumberAnimation { target: zoomView; property: "opacity"; to: 1; duration: LingmoUI.Units.veryLongDuration }
    }

    Timer {
        id: slideshowTimer
        interval: 4000
        repeat: true
        running: viewer.slideshow && !viewer.slideshowPaused && viewer.active && viewer.paths.length > 1
        onTriggered: viewer.next()
    }

    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: {
            if (viewer.pinned)
                restart()
            else
                viewer.controlsVisible = false
        }
    }

    HoverHandler {
        id: stageHover
        cursorShape: viewer.controlsVisible || viewer.cropping ? Qt.ArrowCursor : Qt.BlankCursor
        onPointChanged: {
            if (!viewer.slideshow)
                viewer.poke()
        }
    }

    // ---- Top bar

    // Divider under the header strip (the strip itself is the window header)
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        y: LingmoUI.Units.windowHeaderHeight - 1
        height: 1
        color: LingmoUI.Theme.dividerColor
        opacity: rootWindow.chromeOpacity
        visible: opacity > 0
    }

    // Placed in the window's header strip (Window.headerItem), after the
    // close/minimize/zoom buttons, so it fades with the header
    property Item headerTools: RowLayout {
        visible: viewer.active
        spacing: LingmoUI.Units.smallSpacing / 2

        HoverHandler { id: topHover }
        LingmoUI.IconButton {
            iconName: "view-grid"
            tipText: qsTr("Library")
            enabled: !viewer.cropping
            onClicked: viewer.backRequested()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: LingmoUI.Units.smallSpacing
            Layout.alignment: Qt.AlignVCenter
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: viewer.fileName
                elide: Text.ElideMiddle
                font.pointSize: LingmoUI.Units.fontBody
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                visible: viewer.paths.length > 1
                text: qsTr("%1 of %2").arg(viewer.index + 1).arg(viewer.paths.length)
                font.pointSize: LingmoUI.Units.fontCaption
                color: LingmoUI.Theme.secondaryTextColor
            }
        }

        LingmoUI.IconButton {
            iconName: viewer.favorite ? "qrc:/images/favorite-on.svg"
                                     : LingmoUI.Theme.darkMode ? "qrc:/images/heart-outline-dark.svg" : "qrc:/images/heart-outline-light.svg"
            tipText: viewer.favorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
            enabled: !viewer.cropping
            onClicked: library.setFavorite(viewer.currentPath, !viewer.favorite)
        }
        LingmoUI.IconButton {
            iconName: "object-rotate-left"
            tipText: qsTr("Rotate Left")
            enabled: !viewer.cropping && zoomView.status === Image.Ready
            onClicked: viewer.rotate(-1)
        }
        LingmoUI.IconButton {
            iconName: "object-rotate-right"
            tipText: qsTr("Rotate Right")
            enabled: !viewer.cropping && zoomView.status === Image.Ready
            onClicked: viewer.rotate(1)
        }
        LingmoUI.IconButton {
            iconName: "transform-crop"
            tipText: qsTr("Crop")
            checked: viewer.cropping
            enabled: zoomView.status === Image.Ready && !viewer.info.animated
            onClicked: viewer.cropping ? (viewer.cropping = false) : viewer.startCrop()
        }
        LingmoUI.IconButton {
            iconName: "dialog-information"
            tipText: qsTr("Info")
            checked: viewer.infoVisible
            enabled: !viewer.cropping
            onClicked: viewer.infoVisible = !viewer.infoVisible
        }
        LingmoUI.IconButton {
            iconName: "edit-delete"
            tipText: qsTr("Move to Trash")
            enabled: !viewer.cropping && viewer.currentPath.length > 0
            onClicked: viewer.deleteRequested(viewer.currentPath)
        }
        LingmoUI.IconButton {
            id: moreButton
            iconName: "view-more-symbolic"
            tipText: qsTr("More")
            enabled: !viewer.cropping
            onClicked: moreMenu.popup(moreButton, 0, moreButton.height)
        }
        Item { Layout.preferredWidth: LingmoUI.Units.smallSpacing }
    }

    Menu {
        id: moreMenu
        onAboutToShow: viewer.openWithApps = tools.openWithApps(viewer.currentPath)

        Menu {
            id: openWithMenu
            title: qsTr("Open With")
            enabled: viewer.openWithApps.length > 0

            Instantiator {
                model: viewer.openWithApps
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.name
                    icon.name: modelData.icon
                    onTriggered: {
                        if (!tools.openWith(viewer.currentPath, modelData.id))
                            viewer.toast(tools.lastError)
                    }
                }
                onObjectAdded: (index, object) => openWithMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => openWithMenu.removeItem(object)
            }
        }
        MenuItem {
            text: qsTr("Copy")
            icon.name: "edit-copy"
            onTriggered: {
                tools.copyToClipboard(viewer.currentPath)
                viewer.toast(qsTr("Copied to the clipboard"))
            }
        }
        MenuItem {
            visible: tools.nearbyShareAvailable
            height: visible ? implicitHeight : 0
            text: qsTr("Send to Nearby Device…")
            icon.name: "document-send"
            onTriggered: tools.sendNearby(viewer.currentPath)
        }
        MenuItem {
            text: qsTr("Show in Folder")
            icon.name: "folder-open"
            onTriggered: tools.showInFolder(viewer.currentPath)
        }
        MenuItem {
            text: qsTr("Set as Wallpaper")
            icon.name: "preferences-desktop-wallpaper"
            onTriggered: {
                if (tools.setWallpaper(viewer.currentPath))
                    viewer.toast(qsTr("Wallpaper changed"))
                else
                    viewer.toast(tools.lastError)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Slideshow")
            icon.name: "media-playback-start"
            enabled: viewer.paths.length > 1
            onTriggered: viewer.startSlideshowMode()
        }
        MenuItem {
            text: viewer.fullScreen ? qsTr("Exit Full Screen") : qsTr("Full Screen")
            icon.name: viewer.fullScreen ? "view-restore" : "view-fullscreen"
            onTriggered: viewer.fullScreenRequested()
        }
    }

    // ---- Previous / next

    LingmoUI.IconButton {
        id: prevButton
        anchors.left: parent.left
        anchors.leftMargin: LingmoUI.Units.largeSpacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        size: 44
        iconSize: LingmoUI.Units.iconSizes.smallMedium
        round: true
        flat: false
        iconName: "go-previous"
        tipText: qsTr("Previous")
        visible: opacity > 0
        opacity: viewer.controlsVisible && !viewer.cropping && viewer.paths.length > 1 && (viewer.index > 0 || viewer.slideshow) ? 0.92 : 0
        Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.longDuration } }
        onClicked: viewer.previous()
        HoverHandler { id: sideHoverL }
    }

    LingmoUI.IconButton {
        anchors.right: parent.right
        anchors.rightMargin: (viewer.infoVisible ? infoPanel.width + LingmoUI.Units.largeSpacing * 2 : 0) + LingmoUI.Units.largeSpacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        size: 44
        iconSize: LingmoUI.Units.iconSizes.smallMedium
        round: true
        flat: false
        iconName: "go-next"
        tipText: qsTr("Next")
        visible: opacity > 0
        opacity: viewer.controlsVisible && !viewer.cropping && viewer.paths.length > 1
                 && (viewer.index < viewer.paths.length - 1 || viewer.slideshow) ? 0.92 : 0
        Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.longDuration } }
        onClicked: viewer.next()
        HoverHandler { id: sideHoverR }
    }

    // ---- Bottom: zoom pill, or rotation question, or crop bar

    ColumnLayout {
        id: bottomArea
        anchors.horizontalCenter: zoomView.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: LingmoUI.Units.largeSpacing * 1.5 + (viewer.toastShown ? 64 : 0)
        spacing: LingmoUI.Units.smallSpacing
        opacity: viewer.controlsVisible || viewer.cropping || viewer.rotationPending ? 1 : 0
        Behavior on anchors.bottomMargin { NumberAnimation { duration: LingmoUI.Units.longDuration; easing.type: Easing.OutCubic } }
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: LingmoUI.Units.longDuration } }

        HoverHandler { id: bottomHover }

        LingmoUI.Card {
            id: rotateBar
            Layout.alignment: Qt.AlignHCenter
            visible: viewer.rotationPending && !viewer.cropping
            elevation: 2
            implicitWidth: rotateRow.implicitWidth + LingmoUI.Units.largeSpacing * 2
            implicitHeight: rotateRow.implicitHeight + LingmoUI.Units.smallSpacing * 2

            RowLayout {
                id: rotateRow
                anchors.centerIn: parent
                spacing: LingmoUI.Units.smallSpacing
                Label {
                    Layout.rightMargin: LingmoUI.Units.smallSpacing
                    text: qsTr("Rotated. Keep the change?")
                    font.pointSize: LingmoUI.Units.fontBody
                }
                LingmoUI.ActionButton {
                    text: qsTr("Discard")
                    onClicked: zoomView.quarterTurns = 0
                }
                LingmoUI.ActionButton {
                    text: qsTr("Save…")
                    primary: true
                    onClicked: viewer.askSaveRotation()
                }
            }
        }

        LingmoUI.Card {
            id: cropBar
            Layout.alignment: Qt.AlignHCenter
            visible: viewer.cropping
            elevation: 2
            implicitWidth: cropRow.implicitWidth + LingmoUI.Units.largeSpacing * 2
            implicitHeight: cropRow.implicitHeight + LingmoUI.Units.smallSpacing * 2

            RowLayout {
                id: cropRow
                anchors.centerIn: parent
                spacing: LingmoUI.Units.smallSpacing

                LingmoUI.SegmentedControl {
                    id: aspectControl
                    model: [qsTr("Free"), qsTr("Original"), qsTr("Square"), "4:3", "16:9"]
                    onActivated: index => {
                        const ratios = [0, zoomView.boxWidth / Math.max(1, zoomView.boxHeight), 1, 4 / 3, 16 / 9]
                        cropOverlay.aspect = ratios[index]
                    }
                }
                Item { Layout.preferredWidth: LingmoUI.Units.smallSpacing }
                LingmoUI.ActionButton {
                    text: qsTr("Cancel")
                    onClicked: viewer.cropping = false
                }
                LingmoUI.ActionButton {
                    text: qsTr("Replace Original")
                    visible: tools.canReplace(viewer.currentPath)
                    onClicked: {
                        viewer.pendingQuestion = "replace"
                        choiceDialog.ask(qsTr("Replace the original photo?"),
                                         qsTr("Only the selected area is kept. This can't be undone; “Save Copy” keeps the original."),
                                         [{ id: "replace", text: qsTr("Replace"), destructive: true }])
                    }
                }
                LingmoUI.ActionButton {
                    text: qsTr("Save Copy")
                    primary: true
                    onClicked: viewer.finishCrop(false)
                }
            }
        }

        LingmoUI.Card {
            id: zoomBar
            Layout.alignment: Qt.AlignHCenter
            visible: !viewer.cropping && !viewer.rotationPending
            elevation: 2
            radius: height / 2
            implicitWidth: zoomRow.implicitWidth + LingmoUI.Units.smallSpacing * 2
            implicitHeight: zoomRow.implicitHeight + LingmoUI.Units.smallSpacing

            RowLayout {
                id: zoomRow
                anchors.centerIn: parent
                spacing: 2

                LingmoUI.IconButton {
                    iconName: "zoom-out"
                    tipText: qsTr("Zoom Out")
                    round: true
                    onClicked: zoomView.zoomBy(1 / 1.25)
                }
                Label {
                    Layout.preferredWidth: zoomMetrics.advanceWidth + LingmoUI.Units.smallSpacing
                    horizontalAlignment: Text.AlignHCenter
                    text: Math.round(zoomView.scaleFactor * 100) + " %"
                    font.pointSize: LingmoUI.Units.fontCaption
                    font.features: { "tnum": 1 }
                    TextMetrics { id: zoomMetrics; text: "0000 %"; font.pointSize: LingmoUI.Units.fontCaption }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: zoomView.fitMode ? zoomView.actualSize() : zoomView.fit()
                    }
                    ToolTip.visible: zoomLabelHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: zoomView.fitMode ? qsTr("Actual size") : qsTr("Fit to window")
                    HoverHandler { id: zoomLabelHover }
                }
                LingmoUI.IconButton {
                    iconName: "zoom-in"
                    tipText: qsTr("Zoom In")
                    round: true
                    onClicked: zoomView.zoomBy(1.25)
                }
                LingmoUI.IconButton {
                    iconName: zoomView.fitMode ? "zoom-original" : "zoom-fit-best"
                    tipText: zoomView.fitMode ? qsTr("Actual size") : qsTr("Fit to window")
                    round: true
                    onClicked: zoomView.fitMode ? zoomView.actualSize() : zoomView.fit()
                }
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: LingmoUI.Units.iconSizes.small
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    color: LingmoUI.Theme.dividerColor
                }
                LingmoUI.IconButton {
                    iconName: viewer.slideshow && !viewer.slideshowPaused ? "media-playback-pause" : "media-playback-start"
                    tipText: viewer.slideshow ? (viewer.slideshowPaused ? qsTr("Resume") : qsTr("Pause")) : qsTr("Slideshow")
                    round: true
                    enabled: viewer.paths.length > 1
                    onClicked: {
                        if (viewer.slideshow)
                            viewer.slideshowPaused = !viewer.slideshowPaused
                        else
                            viewer.startSlideshowMode()
                    }
                }
                LingmoUI.IconButton {
                    iconName: viewer.fullScreen ? "view-restore" : "view-fullscreen"
                    tipText: viewer.fullScreen ? qsTr("Exit Full Screen") : qsTr("Full Screen")
                    round: true
                    onClicked: viewer.fullScreenRequested()
                }
            }
        }
    }

    // ---- Info panel

    InfoPanel {
        id: infoPanel
        width: 300
        anchors.top: parent.top
        anchors.topMargin: LingmoUI.Units.windowHeaderHeight + LingmoUI.Units.largeSpacing
        anchors.bottom: parent.bottom
        anchors.bottomMargin: LingmoUI.Units.largeSpacing
        x: viewer.infoVisible ? parent.width - width - LingmoUI.Units.largeSpacing : parent.width + LingmoUI.Units.largeSpacing
        visible: x < parent.width
        info: viewer.info
        favorite: viewer.favorite
        onCloseRequested: viewer.infoVisible = false
        onShowInFolder: tools.showInFolder(viewer.currentPath)
        Behavior on x { NumberAnimation { duration: LingmoUI.Units.longDuration; easing.type: Easing.OutCubic } }
    }

    ChoiceDialog {
        id: choiceDialog
        onChosen: id => {
            const question = viewer.pendingQuestion
            viewer.pendingQuestion = ""
            if (question === "rotate" && id.length > 0)
                viewer.saveRotation(id === "lossless")
            else if (question === "replace" && id === "replace")
                viewer.finishCrop(true)
        }
    }

    // ---- Keyboard

    readonly property bool keysEnabled: active && !choiceDialog.visible && !moreMenu.visible

    Shortcut { sequences: ["Right", "PgDown"]; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.next() }
    Shortcut { sequences: ["Left", "PgUp"]; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.previous() }
    Shortcut { sequence: "Home"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.go(0) }
    Shortcut { sequence: "End"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.go(viewer.paths.length - 1) }
    Shortcut { sequence: "Ctrl+0"; enabled: viewer.keysEnabled; onActivated: zoomView.fit() }
    Shortcut { sequence: "Ctrl+1"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: zoomView.actualSize() }
    Shortcut { sequences: [StandardKey.ZoomIn, "Ctrl+="]; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: zoomView.zoomBy(1.25) }
    Shortcut { sequences: [StandardKey.ZoomOut]; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: zoomView.zoomBy(1 / 1.25) }
    Shortcut { sequences: ["F11", "F"]; enabled: viewer.keysEnabled; onActivated: viewer.fullScreenRequested() }
    Shortcut { sequence: "Ctrl+R"; enabled: viewer.keysEnabled; onActivated: viewer.rotate(1) }
    Shortcut { sequence: "Ctrl+Shift+R"; enabled: viewer.keysEnabled; onActivated: viewer.rotate(-1) }
    Shortcut { sequence: "Ctrl+I"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.infoVisible = !viewer.infoVisible }
    Shortcut { sequences: [StandardKey.Copy]; enabled: viewer.keysEnabled; onActivated: { tools.copyToClipboard(viewer.currentPath); viewer.toast(qsTr("Copied to the clipboard")) } }
    Shortcut { sequence: "Delete"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.deleteRequested(viewer.currentPath) }
    Shortcut { sequence: "F5"; enabled: viewer.keysEnabled && !viewer.cropping; onActivated: viewer.slideshow ? viewer.stopSlideshow() : viewer.startSlideshowMode() }
    Shortcut { sequence: "Space"; enabled: viewer.keysEnabled && viewer.slideshow; onActivated: viewer.slideshowPaused = !viewer.slideshowPaused }
    Shortcut { sequence: "Return"; enabled: viewer.keysEnabled && viewer.cropping; onActivated: viewer.finishCrop(false) }
    Shortcut {
        sequence: "Esc"
        enabled: viewer.keysEnabled
        onActivated: {
            if (viewer.cropping)
                viewer.cropping = false
            else if (viewer.slideshow)
                viewer.stopSlideshow()
            else if (viewer.fullScreen)
                viewer.fullScreenRequested()
            else if (viewer.infoVisible)
                viewer.infoVisible = false
            else
                viewer.backRequested()
        }
    }
}
