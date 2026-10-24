cmake_minimum_required(VERSION 3.16)
project(lingmo-photos VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

option(BUILD_TESTING "Build the unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Concurrent DBus Qml Quick LinguistTools)
# "Open with": the applications for a MIME type, started the KDE/XDG way
find_package(KF6Service REQUIRED)
find_package(KF6KIO REQUIRED)

# Library core (EXIF, scanning, date groups, thumbnails, trash), shared with the tests
add_library(photos-core STATIC
    src/dategroups.cpp
    src/exif.cpp
    src/photolibrary.cpp
    src/scanner.cpp
    src/thumbnailcache.cpp
    src/trash.cpp
)
target_include_directories(photos-core PUBLIC src)
target_link_libraries(photos-core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent)

add_executable(lingmo-photos
    src/main.cpp
    src/imagetools.cpp
    src/thumbnailprovider.cpp
    qml.qrc
)
target_link_libraries(lingmo-photos PRIVATE
    photos-core
    Qt6::Core Qt6::Gui Qt6::DBus Qt6::Qml Qt6::Quick
    KF6::Service KF6::KIOGui
)

set(TRANSLATIONS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/lingmo-photos/translations)
target_compile_definitions(lingmo-photos PRIVATE
    TRANSLATIONS_DIR="${TRANSLATIONS_DIR}"
    LINGMO_PHOTOS_VERSION="${PROJECT_VERSION}"
)
# lrelease only: building never rewrites the .ts sources
qt_add_translation(QM_FILES translations/lingmo-photos_pt_BR.ts)
add_custom_target(translations ALL DEPENDS ${QM_FILES})
install(FILES ${QM_FILES} DESTINATION ${TRANSLATIONS_DIR})

install(TARGETS lingmo-photos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Lingmo Intents: photos.open on com.lingmo.Photos /Intents (`lingmo-photos --intents`)
find_package(LingmoIntents 1.0 QUIET)
if(LingmoIntents_FOUND)
    target_sources(lingmo-photos PRIVATE src/photosintents.cpp src/photosintents.h)
    target_compile_definitions(lingmo-photos PRIVATE HAVE_LINGMO_INTENTS)
    target_link_libraries(lingmo-photos PRIVATE LingmoIntents::LingmoIntents)
    lingmo_install_intents(MANIFEST data/intents/lingmo-photos.json SERVICE data/com.lingmo.Photos.service)
else()
    message(WARNING "LingmoIntents not found: Photos is built without app actions (intents)")
endif()
install(FILES lingmo-photos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(test tst_dategroups tst_thumbnails tst_scanner tst_trash tst_exif)
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE photos-core Qt6::Test)
        add_test(NAME ${test} COMMAND ${test})
        # Every test points HOME and the XDG folders to a temporary folder
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LC_ALL=C")
    endforeach()
endif()
