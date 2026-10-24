#pragma once

// photos.open (Lingmo Intents) on com.lingmo.Photos /Intents.
// `lingmo-photos --intents` (D-Bus activation) answers without a window and
// opens Photos the usual way.
namespace PhotosIntents {
bool requested(int argc, char *argv[]);
int run(int argc, char *argv[]);
}
