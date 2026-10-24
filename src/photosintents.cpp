#include "photosintents.h"

#include <LingmoIntents/intentservice.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QTranslator>

using LingmoIntents::IntentCall;
using LingmoIntents::IntentResult;

namespace PhotosIntents {

bool requested(int argc, char *argv[])
{
    return argc == 2 && qstrcmp(argv[1], "--intents") == 0;
}

int run(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("lingmo-photos"), QStringLiteral("_"), QStringLiteral(TRANSLATIONS_DIR)))
        app.installTranslator(&translator);
    LingmoIntents::IntentService intents(QStringLiteral("lingmo-photos"));
    intents.handle(QStringLiteral("photos.open"), [](const IntentCall &call) {
        const QFileInfo fi(call.params().value(QStringLiteral("path")).toString());
        if (!fi.exists())
            return IntentResult::failed(QObject::tr("File not found: %1").arg(fi.filePath()));
        QStringList args;
        if (call.params().value(QStringLiteral("slideshow")).toBool())
            args << QStringLiteral("--slideshow");
        if (fi.isDir()) {
            args << QStringLiteral("--library") << fi.absoluteFilePath();
        } else {
            if (QImageReader::imageFormat(fi.absoluteFilePath()).isEmpty())
                return IntentResult::failed(QObject::tr("This file is not a picture."));
            args << fi.absoluteFilePath();
        }
        QString error;
        if (!LingmoIntents::spawnDetached(QStringLiteral("lingmo-photos"), args, &error))
            return IntentResult::failed(error);
        return IntentResult::ok({{QStringLiteral("path"), fi.absoluteFilePath()}});
    });
    if (!intents.publish(LingmoIntents::IntentService::Headless))
        return 1;
    intents.setIdleQuit(20000);
    return app.exec();
}

}
