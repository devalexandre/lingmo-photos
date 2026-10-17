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

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>
#include <QUrl>

#include "imagetools.h"
#include "photolibrary.h"
#include "scanner.h"
#include "thumbnailprovider.h"
#include "trash.h"

// The Lingmo platform theme doesn't always hand the icon theme to QGuiApplication
// processes (the dock works around it too): use the one set in Lingmo Settings.
static void applyIconTheme()
{
    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != QLatin1String("hicolor"))
        return;
    QSettings theme(QStringLiteral("lingmoos"), QStringLiteral("theme"));
    const bool dark = theme.value(QStringLiteral("DarkMode"), false).toBool();
    const QStringList candidates = {
        theme.value(dark ? QStringLiteral("DarkIconTheme") : QStringLiteral("IconTheme")).toString(),
        dark ? QStringLiteral("Lingmo-dark") : QStringLiteral("Lingmo"),
        dark ? QStringLiteral("Crule-dark") : QStringLiteral("Crule"),
        QStringLiteral("breeze"),
    };
    for (const QString &name : candidates) {
        if (!name.isEmpty()
            && !QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                       QStringLiteral("icons/%1/index.theme").arg(name)).isEmpty()) {
            QIcon::setThemeName(name);
            break;
        }
    }
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("lingmo-photos"));
    app.setApplicationVersion(QStringLiteral(LINGMO_PHOTOS_VERSION));
    app.setDesktopFileName(QStringLiteral("lingmo-photos"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("lingmo-photos"),
                                       QIcon::fromTheme(QStringLiteral("image-viewer"))));
    applyIconTheme();

    // Installed translations, or the ones next to the binary in a build tree
    QTranslator translator;
    for (const QString &dir : {QStringLiteral(TRANSLATIONS_DIR), QCoreApplication::applicationDirPath()}) {
        if (translator.load(QLocale(), QStringLiteral("lingmo-photos"), QStringLiteral("_"), dir)) {
            app.installTranslator(&translator);
            break;
        }
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lingmo Photos: image viewer and photo library"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption libraryOption(QStringLiteral("library"),
                                     QStringLiteral("Show the library of this folder instead of the Pictures folder"),
                                     QStringLiteral("folder"));
    QCommandLineOption slideshowOption(QStringLiteral("slideshow"), QStringLiteral("Start a slideshow of the given files"));
    parser.addOption(libraryOption);
    parser.addOption(slideshowOption);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Images to open"), QStringLiteral("[files...]"));
    parser.process(app);

    QStringList files;
    for (const QString &arg : parser.positionalArguments()) {
        const QUrl url = QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile);
        if (!url.isLocalFile())
            continue;
        const QFileInfo info(url.toLocalFile());
        if (info.isDir()) {
            // A folder: open its first image
            const QStringList images = Scanner::folderImages(info.absoluteFilePath());
            if (!images.isEmpty())
                files << images.first();
        } else if (info.exists()) {
            files << info.absoluteFilePath();
        }
    }

    PhotoLibrary library(parser.value(libraryOption));
    ImageTools tools;
    Trash trash;
    QObject::connect(&tools, &ImageTools::fileChanged, &library, [&library](const QString &path) {
        library.addPaths({path});
    });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("tools"), &tools);
    engine.rootContext()->setContextProperty(QStringLiteral("trash"), &trash);
    engine.rootContext()->setContextProperty(QStringLiteral("initialFiles"), files);
    engine.rootContext()->setContextProperty(QStringLiteral("startSlideshow"), parser.isSet(slideshowOption));
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
