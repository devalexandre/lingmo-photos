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

// Library: sidebar (Photos, Favorites, Folders) + grid grouped by month
Item {
    id: page

    signal openPhoto(var paths, int index)
    signal startSlideshow(var paths, int index)

    readonly property int pagePhotos: 0
    readonly property int pageFavorites: 1
    readonly property int pageFolders: 2
    // Folder opened from the Folders page ("" = the folder list)
    property string openFolder: ""
    property string openFolderName: ""
    property bool selecting: library.selectedCount > 0
    // Thumbnail size: 0 small, 1 medium, 2 large
    property int gridSize: 1

    readonly property bool showingGrid: sideBar.currentIndex !== pageFolders || openFolder.length > 0

    function reveal(path) {
        const index = library.indexOfPath(path)
        if (index >= 0)
            grid.positionAtPhoto(index)
    }

    function syncFilters() {
        library.clearSelection()
        library.favoritesOnly = sideBar.currentIndex === pageFavorites
        library.folderFilter = sideBar.currentIndex === pageFolders ? openFolder : ""
    }

    function deleteSelection() {
        const paths = library.selectedPaths()
        if (paths.length > 0)
            rootWindow.trashFiles(paths)
    }

    function slideshow() {
        const selected = library.selectedPaths()
        if (selected.length > 1)
            page.startSlideshow(selected, 0)
        else if (library.count > 0)
            page.startSlideshow(library.visiblePaths(), 0)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        LingmoUI.SettingsSideBar {
            id: sideBar
            Layout.fillHeight: true
            title: qsTr("Photos")
            model: ListModel {
                id: sideModel
                ListElement { title: qsTr("Library"); iconSource: "qrc:/images/photos.svg"; iconColor: "#0A84FF"; badge: ""; section: "" }
                ListElement { title: qsTr("Favorites"); iconSource: "qrc:/images/favorites.svg"; iconColor: "#FF375F"; badge: ""; section: "" }
                ListElement { title: qsTr("Folders"); iconSource: "qrc:/images/folders.svg"; iconColor: "#FF9F0A"; badge: ""; section: "" }
            }
            onCurrentIndexChanged: {
                page.openFolder = ""
                page.syncFilters()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: LingmoUI.Units.largeSpacing * 2
                anchors.rightMargin: LingmoUI.Units.largeSpacing * 2
                spacing: LingmoUI.Units.largeSpacing

                // Title and tools, in the window's header strip (the strip only
                // takes the presses nothing here takes)
                LingmoUI.PageHeader {
                    id: pageHeader
                    Layout.fillWidth: true
                    Layout.topMargin: LingmoUI.Units.smallSpacing
                    backVisible: page.openFolder.length > 0
                    backTipText: qsTr("Back to Folders")
                    onBack: {
                        page.openFolder = ""
                        page.syncFilters()
                    }
                    title: page.openFolder.length > 0 ? page.openFolderName
                         : sideBar.currentIndex === page.pageFavorites ? qsTr("Favorites")
                         : sideBar.currentIndex === page.pageFolders ? qsTr("Folders")
                         : qsTr("Library")
                    subtitle: {
                        if (library.scanning && library.totalCount === 0)
                            return qsTr("Looking for photos…")
                        if (sideBar.currentIndex === page.pageFolders && page.openFolder.length === 0)
                            return foldersPage.count === 1 ? qsTr("1 folder") : qsTr("%1 folders").arg(foldersPage.count)
                        return rootWindow.photoCount(library.count)
                    }

                    TextField {
                        id: searchField
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: LingmoUI.Units.smallControlHeight + 2
                        visible: page.showingGrid
                        placeholderText: qsTr("Search name, folder or date")
                        selectByMouse: true
                        onTextChanged: searchTimer.restart()
                        Keys.onEscapePressed: text = ""
                        Timer {
                            id: searchTimer
                            interval: 200
                            onTriggered: library.searchText = searchField.text
                        }
                    }

                    LingmoUI.SegmentedControl {
                        visible: page.showingGrid
                        model: [{ iconName: "view-list-icons", text: "" },
                                { iconName: "view-grid", text: "" },
                                { iconName: "zoom-in", text: "" }]
                        currentIndex: page.gridSize
                        onActivated: index => page.gridSize = index
                        ToolTip.visible: sizeHover.hovered
                        ToolTip.delay: 600
                        ToolTip.text: qsTr("Thumbnail size")
                        HoverHandler { id: sizeHover }
                    }

                    LingmoUI.IconButton {
                        iconName: "media-playback-start"
                        tipText: qsTr("Slideshow")
                        visible: page.showingGrid
                        enabled: library.count > 0
                        onClicked: page.slideshow()
                    }

                    LingmoUI.IconButton {
                        iconName: "view-refresh"
                        tipText: qsTr("Look for new photos")
                        enabled: !library.scanning
                        onClicked: library.rescan()
                    }
                }

                // Selection actions
                LingmoUI.Card {
                    id: selectionBar
                    Layout.fillWidth: true
                    visible: page.selecting
                    implicitHeight: selectionRow.implicitHeight + LingmoUI.Units.smallSpacing * 2
                    elevation: 1

                    RowLayout {
                        id: selectionRow
                        anchors.fill: parent
                        anchors.leftMargin: selectionBar.padding
                        anchors.rightMargin: LingmoUI.Units.smallSpacing
                        spacing: LingmoUI.Units.smallSpacing

                        Label {
                            Layout.fillWidth: true
                            text: library.selectedCount === 1 ? qsTr("1 photo selected") : qsTr("%1 photos selected").arg(library.selectedCount)
                            font.pointSize: LingmoUI.Units.fontBody
                            font.bold: true
                        }
                        LingmoUI.ActionButton {
                            text: qsTr("Select All")
                            onClicked: library.selectAll()
                        }
                        LingmoUI.ActionButton {
                            readonly property bool allFavorite: library.revision >= 0 && library.allSelectedFavorite()
                            text: allFavorite ? qsTr("Remove from Favorites") : qsTr("Add to Favorites")
                            iconName: "emblem-favorite"
                            onClicked: library.setFavorites(library.selectedPaths(), !allFavorite)
                        }
                        LingmoUI.ActionButton {
                            text: qsTr("Move to Trash")
                            iconName: "edit-delete"
                            destructive: true
                            onClicked: page.deleteSelection()
                        }
                        LingmoUI.IconButton {
                            iconName: "window-close"
                            tipText: qsTr("Clear selection")
                            onClicked: library.clearSelection()
                        }
                    }
                }

                LingmoUI.Banner {
                    Layout.fillWidth: true
                    type: LingmoUI.Banner.Info
                    visible: library.scanning && library.totalCount > 0
                    closable: false
                    title: library.scanning && library.totalCount > 0 ? qsTr("Looking for new photos…") : ""
                    text: library.scanning && library.totalCount > 0 ? qsTr("Found so far: %1.").arg(rootWindow.photoCount(library.totalCount)) : ""

                    LingmoUI.Spinner { size: LingmoUI.Units.iconSizes.small; running: library.scanning }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: page.showingGrid ? 0 : 1

                    Item {
                        PhotoGrid {
                            id: grid
                            anchors.fill: parent
                            visible: library.count > 0
                            minCell: page.gridSize === 0 ? 110 : page.gridSize === 1 ? 170 : 260
                            onOpenPhoto: index => page.openPhoto(library.visiblePaths(), index)
                            onDeletePressed: page.deleteSelection()
                            onZoomRequested: delta => page.gridSize = Math.max(0, Math.min(2, page.gridSize + delta))
                        }

                        LingmoUI.EmptyState {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - LingmoUI.Units.largeSpacing * 4, 420)
                            visible: library.count === 0
                            busy: library.scanning
                            iconName: library.searchText.length > 0 ? "edit-find"
                                    : sideBar.currentIndex === page.pageFavorites ? "emblem-favorite"
                                    : "folder-pictures"
                            title: library.scanning ? qsTr("Looking for photos…")
                                 : library.searchText.length > 0 ? qsTr("No results")
                                 : sideBar.currentIndex === page.pageFavorites ? qsTr("No favorites yet")
                                 : qsTr("No photos yet")
                            text: library.scanning ? ""
                                : library.searchText.length > 0 ? qsTr("Nothing matches “%1”. Try a file name, a month or a year.").arg(library.searchText)
                                : sideBar.currentIndex === page.pageFavorites ? qsTr("Mark photos with the heart to find them here.")
                                : qsTr("Photos and pictures you save to %1 show up here.").arg(tools.prettyPath(library.rootFolder))
                            actionText: library.scanning || library.searchText.length > 0
                                        || sideBar.currentIndex === page.pageFavorites ? "" : qsTr("Open Pictures Folder")
                            actionPrimary: false
                            onActionTriggered: tools.showInFolder(library.rootFolder)
                        }
                    }

                    FoldersPage {
                        id: foldersPage
                        onOpenFolder: (path, name) => {
                            page.openFolder = path
                            page.openFolderName = name
                            page.syncFilters()
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: page.visible && searchField.visible
        onActivated: searchField.forceActiveFocus()
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        enabled: page.visible && page.showingGrid && !searchField.activeFocus
        onActivated: library.selectAll()
    }
    Shortcut {
        sequence: "Esc"
        enabled: page.visible && page.selecting
        onActivated: library.clearSelection()
    }
    Shortcut {
        sequence: "F5"
        enabled: page.visible
        onActivated: library.rescan()
    }
}
