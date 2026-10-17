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

LingmoUI.Window {
    id: rootWindow

    width: 1120
    height: 740
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: viewerOpen ? viewer.fileName : qsTr("Photos")

    // The viewer is immersive: a dark stage whatever the theme (like Preview/Photos)
    readonly property color stageColor: Qt.rgba(0.07, 0.07, 0.08, 1)
    property bool viewerOpen: initialFiles.length > 0
    // Library loads on demand: opening a file from the file manager stays instant
    property bool libraryWanted: initialFiles.length === 0
    // Appended to thumbnail urls; bumped when a file is rewritten (rotate, crop)
    property int thumbVersion: 0

    background.opacity: viewerOpen ? 1.0 : (LingmoUI.Theme.darkMode ? 0.88 : 0.97)
    background.color: viewerOpen ? stageColor : LingmoUI.Theme.backgroundColor
    header.height: LingmoUI.Units.windowHeaderHeight
    contentTopMargin: 0
    // In the viewer the header strip holds its tools and fades with the controls;
    // the library puts its tools in its own page header, under the strip
    headerItem: viewerOpen ? viewer.headerTools : null
    headerBackground.color: viewerOpen ? Qt.rgba(LingmoUI.Theme.backgroundColor.r, LingmoUI.Theme.backgroundColor.g,
                                                 LingmoUI.Theme.backgroundColor.b, 0.94)
                                       : "transparent"
    header.opacity: chromeOpacity
    header.visible: chromeOpacity > 0
    property real chromeOpacity: !viewerOpen || viewer.controlsVisible || viewer.cropping ? 1 : 0
    Behavior on chromeOpacity { NumberAnimation { duration: LingmoUI.Units.longDuration } }

    LingmoUI.WindowBlur {
        view: rootWindow
        geometry: Qt.rect(rootWindow.x, rootWindow.y, rootWindow.width, rootWindow.height)
        windowRadius: rootWindow.windowRadius
        enabled: !rootWindow.viewerOpen
    }

    function thumbUrl(path) {
        return "image://thumb/" + encodeURIComponent(path) + "?v=" + thumbVersion
    }

    function photoCount(n) {
        return n === 1 ? qsTr("1 photo") : qsTr("%1 photos").arg(n)
    }

    function openViewer(paths, index, slideshow) {
        viewer.open(paths, index, !!slideshow)
        viewerOpen = true
    }

    function showLibrary() {
        const current = viewer.currentPath
        if (rootWindow.isFullScreen)
            rootWindow.showNormal()
        viewer.stopSlideshow()
        viewerOpen = false
        libraryWanted = true
        library.start()
        if (libraryLoader.item && current.length > 0)
            libraryLoader.item.reveal(current)
    }

    function toggleFullScreen() {
        if (rootWindow.isFullScreen)
            rootWindow.showNormal()
        else
            rootWindow.showFullScreen()
    }

    // Moves files to the trash and offers "Undo" in a toast
    function trashFiles(paths) {
        const batch = trash.moveToTrash(paths)
        if (batch <= 0) {
            showPassiveNotification(qsTr("Couldn't move to the trash"), "short")
            return []
        }
        const moved = paths.filter(p => trash.lastFailed().indexOf(p) < 0)
        const message = moved.length === 1 ? qsTr("Moved to the trash")
                                           : qsTr("%1 photos moved to the trash").arg(moved.length)
        showPassiveNotification(message, "long", qsTr("Undo"), function() {
            const restored = trash.undo(batch)
            viewer.restore(restored)
            viewer.toastShown = false
        })
        if (viewerOpen)
            viewer.noteToast(12000)
        return moved
    }

    Connections {
        target: trash
        function onTrashed(paths) { library.removePaths(paths) }
        function onRestored(paths) { library.addPaths(paths) }
    }

    Connections {
        target: tools
        function onFileChanged(path) { rootWindow.thumbVersion++ }
    }

    Loader {
        id: libraryLoader
        anchors.fill: parent
        active: rootWindow.libraryWanted
        visible: !rootWindow.viewerOpen
        sourceComponent: LibraryPage {
            onOpenPhoto: (paths, index) => rootWindow.openViewer(paths, index, false)
            onStartSlideshow: (paths, index) => rootWindow.openViewer(paths, index, true)
        }
        onLoaded: library.start()
    }

    Viewer {
        id: viewer
        anchors.fill: parent
        visible: rootWindow.viewerOpen
        active: rootWindow.viewerOpen
        stageColor: rootWindow.stageColor
        onBackRequested: rootWindow.showLibrary()
        onFullScreenRequested: rootWindow.toggleFullScreen()
        onDeleteRequested: path => {
            if (rootWindow.trashFiles([path]).length > 0)
                viewer.removeCurrent()
        }
        onEmptied: rootWindow.showLibrary()
    }

    Component.onCompleted: {
        if (initialFiles.length > 0) {
            // One file: browse its folder. Several: just those.
            const list = Array.from(initialFiles.length === 1 ? tools.folderImages(initialFiles[0]) : initialFiles)
            let index = list.indexOf(initialFiles[0])
            if (index < 0) {
                list.unshift(initialFiles[0])
                index = 0
            }
            openViewer(list, index, startSlideshow)
        }
    }
}
