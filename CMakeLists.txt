cmake_minimum_required(VERSION 3.16)

project(LearningBox VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Sql TextToSpeech Multimedia Concurrent Network)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appLearningBox
    main.cpp
)

qt_add_qml_module(appLearningBox
    URI LearningBox
    SOURCES
        src/leitner.h
        src/card.h
        src/cardstore.h        src/cardstore.cpp
        src/cardlistmodel.h    src/cardlistmodel.cpp
        src/reviewsession.h    src/reviewsession.cpp
        src/speaker.h          src/speaker.cpp
        src/wordpack.h         src/wordpack.cpp
        src/wordpacks.h        src/wordpacks.cpp
        src/starterdeck.h      src/starterdeck.cpp
        src/appmode.h          src/appmode.cpp
        src/cardimages.h       src/cardimages.cpp
        src/ocrengine.h        src/ocrengine.cpp
        src/ocr/ocrtypes.h
        src/ocr/ocr.h          src/ocr/ocr.cpp
        src/ocr/textselect.h   src/ocr/textselect.cpp
        src/translator.h       src/translator.cpp
        src/devicetilt.h       src/devicetilt.cpp
        src/cloudocr.h         src/cloudocr.cpp
        src/ocr/azureread.h    src/ocr/azureread.cpp
        src/translation/googletranslate.h src/translation/googletranslate.cpp
        src/translation/tatoeba.h         src/translation/tatoeba.cpp
        src/deckexchange.h     src/deckexchange.cpp
        src/exchange/deckformats.h src/exchange/deckformats.cpp
    QML_FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/ReviewPage.qml
        qml/CardEditPage.qml
        qml/CardListPage.qml
        qml/SpeakButton.qml
        qml/WordPacksPage.qml
        qml/WordPackChapterPage.qml
        qml/MeaningText.qml
        qml/LensPage.qml
        qml/SettingsPage.qml
        qml/ExampleText.qml
        qml/BoxPage.qml
        qml/GearIcon.qml
        qml/SendCardsPage.qml
        qml/GetCardsPage.qml
        qml/SetupPage.qml
        qml/ModeComparison.qml
        qml/ResetDialog.qml
        qml/HintLabel.qml
        qml/CameraShotPage.qml
        qml/EditIcon.qml
        qml/HelpIcon.qml
        qml/HelpPage.qml
)

# Built-in vocabulary packs (read at runtime from :/data/wordpacks/...)
qt_add_resources(appLearningBox "wordpacks"
    PREFIX "/"
    FILES data/wordpacks/netzwerk_neu_a1.tsv data/starter/starter_100.tsv
)

target_include_directories(appLearningBox PRIVATE src)

# ---- OCR (Tesseract + Leptonica, static) ----
# Prebuilt per platform into 3rdparty/install/<platform>; see 3rdparty/README.md.
# Without them the app still builds: the Lens page then shows what is missing and where to get it.
if (ANDROID)
    set(LB_OCR_PLATFORM "android-${ANDROID_ABI}")
else()
    set(LB_OCR_PLATFORM "${CMAKE_SYSTEM_NAME}")
endif()
set(LB_OCR_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/install/${LB_OCR_PLATFORM}" CACHE PATH
    "Install prefix containing Leptonica and Tesseract for this platform")
set(Leptonica_DIR "${LB_OCR_PREFIX}/lib/cmake/leptonica")
set(Tesseract_DIR "${LB_OCR_PREFIX}/lib/cmake/tesseract")
if (ANDROID)
    # Tesseract's Android build links Google cpu_features (NDK compat layer).
    set(CpuFeatures_DIR "${LB_OCR_PREFIX}/lib/cmake/CpuFeatures")
    set(CpuFeaturesNdkCompat_DIR "${LB_OCR_PREFIX}/lib/cmake/CpuFeaturesNdkCompat")
    find_package(CpuFeaturesNdkCompat CONFIG QUIET)
endif()
find_package(Tesseract CONFIG QUIET)
if (Tesseract_FOUND)
    message(STATUS "LearningBox: OCR enabled (Tesseract ${Tesseract_VERSION}, ${LB_OCR_PREFIX})")
    # The exported target carries no include path; add it explicitly.
    set_property(TARGET Tesseract::libtesseract APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${Tesseract_INCLUDE_DIRS}")
    target_link_libraries(appLearningBox PRIVATE Tesseract::libtesseract)
    target_compile_definitions(appLearningBox PRIVATE LB_HAVE_OCR)
else()
    message(WARNING "LearningBox: Tesseract not found in ${LB_OCR_PREFIX} - building without OCR")
endif()

# German OCR model (tessdata_fast, Apache-2.0), available as :/ocr/tessdata/deu.traineddata
set_source_files_properties(data/tessdata/deu.traineddata PROPERTIES QT_RESOURCE_ALIAS tessdata/deu.traineddata)
qt_add_resources(appLearningBox "ocrdata"
    PREFIX "/ocr"
    FILES data/tessdata/deu.traineddata
)

set_target_properties(appLearningBox PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER se.morteza.learningbox
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    QT_ANDROID_PACKAGE_NAME se.morteza.learningbox
    QT_ANDROID_APP_NAME "Learning Box"
    QT_ANDROID_VERSION_NAME ${PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE 1
)

# Custom AndroidManifest.xml (needed so TTS can see the system TTS engine on Android 11+).
if (ANDROID AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
    set_property(TARGET appLearningBox APPEND PROPERTY
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)
endif()

target_link_libraries(appLearningBox
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::TextToSpeech Qt6::Multimedia Qt6::Concurrent Qt6::Network
)

# Import/export: Qt's zip reader/writer are private API (QtCore in new Qt, QtGui in older).
find_package(Qt6 REQUIRED COMPONENTS CorePrivate GuiPrivate)
target_link_libraries(appLearningBox PRIVATE Qt6::CorePrivate Qt6::GuiPrivate)

# Phone tilt for Lens (Qt Sensors: Maintenance Tool -> Additional Libraries -> Qt Sensors).
# Optional: without it Lens detects the orientation from the picture instead.
find_package(Qt6 COMPONENTS Sensors QUIET)
if (Qt6Sensors_FOUND)
    target_link_libraries(appLearningBox PRIVATE Qt6::Sensors)
    target_compile_definitions(appLearningBox PRIVATE LB_HAVE_SENSORS)
else()
    message(STATUS "LearningBox: Qt Sensors not found - Lens uses picture-based orientation only")
endif()

# HTTPS on Android: Qt loads OpenSSL at runtime, Android ships none -> bundle KDAB's prebuilt
# libssl_3/libcrypto_3 (3rdparty/android_openssl, Apache-2.0). Without them online translation
# fails with a TLS error and Lens falls back to offline translations.
if (ANDROID)
    set(LB_OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/android_openssl/ssl_3/${ANDROID_ABI}")
    if (EXISTS "${LB_OPENSSL_DIR}/libssl_3.so")
        set_property(TARGET appLearningBox APPEND PROPERTY QT_ANDROID_EXTRA_LIBS
            "${LB_OPENSSL_DIR}/libcrypto_3.so" "${LB_OPENSSL_DIR}/libssl_3.so")
    else()
        message(WARNING "LearningBox: no OpenSSL for ${ANDROID_ABI} in 3rdparty/android_openssl - online translation will fail")
    endif()
endif()

# ---- Unit tests (desktop only) ----
option(LEARNINGBOX_BUILD_TESTS "Build unit tests" ON)
if (LEARNINGBOX_BUILD_TESTS AND NOT ANDROID AND NOT IOS)
    find_package(Qt6 COMPONENTS Test)
    if (Qt6Test_FOUND)
        enable_testing()
        qt_add_executable(tst_leitner tests/tst_leitner.cpp)
        target_include_directories(tst_leitner PRIVATE src)
        target_link_libraries(tst_leitner PRIVATE Qt6::Test)
        set_target_properties(tst_leitner PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_leitner COMMAND tst_leitner)

        qt_add_executable(tst_wordpack tests/tst_wordpack.cpp src/wordpack.cpp)
        target_include_directories(tst_wordpack PRIVATE src)
        target_link_libraries(tst_wordpack PRIVATE Qt6::Test)
        target_compile_definitions(tst_wordpack PRIVATE
            LB_WORDPACK_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/wordpacks/netzwerk_neu_a1.tsv")
        set_target_properties(tst_wordpack PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_wordpack COMMAND tst_wordpack)

        qt_add_executable(tst_cardimages tests/tst_cardimages.cpp src/cardimages.cpp)
        target_include_directories(tst_cardimages PRIVATE src)
        target_link_libraries(tst_cardimages PRIVATE Qt6::Test Qt6::Gui)
        set_target_properties(tst_cardimages PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_cardimages COMMAND tst_cardimages)

        qt_add_executable(tst_textselect tests/tst_textselect.cpp src/ocr/textselect.cpp)
        target_include_directories(tst_textselect PRIVATE src)
        target_link_libraries(tst_textselect PRIVATE Qt6::Test)
        set_target_properties(tst_textselect PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_textselect COMMAND tst_textselect)

        qt_add_executable(tst_deckformats tests/tst_deckformats.cpp src/exchange/deckformats.cpp)
        target_include_directories(tst_deckformats PRIVATE src)
        target_link_libraries(tst_deckformats PRIVATE Qt6::Test Qt6::Gui Qt6::Sql Qt6::CorePrivate Qt6::GuiPrivate)
        set_target_properties(tst_deckformats PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_deckformats COMMAND tst_deckformats)

        qt_add_executable(tst_deckexchange tests/tst_deckexchange.cpp src/deckexchange.cpp src/deckexchange.h
            src/exchange/deckformats.cpp src/cardstore.cpp src/cardstore.h src/cardimages.cpp)
        target_include_directories(tst_deckexchange PRIVATE src)
        target_link_libraries(tst_deckexchange PRIVATE Qt6::Test Qt6::Gui Qt6::Sql Qt6::Qml Qt6::Network
            Qt6::CorePrivate Qt6::GuiPrivate)
        set_target_properties(tst_deckexchange PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_deckexchange COMMAND tst_deckexchange)

        qt_add_executable(tst_tatoeba tests/tst_tatoeba.cpp src/translation/tatoeba.cpp)
        target_include_directories(tst_tatoeba PRIVATE src)
        target_link_libraries(tst_tatoeba PRIVATE Qt6::Test)
        set_target_properties(tst_tatoeba PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_tatoeba COMMAND tst_tatoeba)

        qt_add_executable(tst_azureread tests/tst_azureread.cpp src/ocr/azureread.cpp)
        target_include_directories(tst_azureread PRIVATE src)
        target_link_libraries(tst_azureread PRIVATE Qt6::Test Qt6::Gui)
        set_target_properties(tst_azureread PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_azureread COMMAND tst_azureread)

        qt_add_executable(tst_devicetilt tests/tst_devicetilt.cpp src/devicetilt.cpp src/devicetilt.h)
        target_include_directories(tst_devicetilt PRIVATE src)
        target_link_libraries(tst_devicetilt PRIVATE Qt6::Test Qt6::Gui Qt6::Qml)
        set_target_properties(tst_devicetilt PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_devicetilt COMMAND tst_devicetilt)

        # Pure merge/row helpers of src/ocr/ocr.cpp (builds without Tesseract)
        qt_add_executable(tst_ocrmerge tests/tst_ocrmerge.cpp src/ocr/ocr.cpp)
        target_include_directories(tst_ocrmerge PRIVATE src)
        target_link_libraries(tst_ocrmerge PRIVATE Qt6::Test Qt6::Gui)
        set_target_properties(tst_ocrmerge PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_ocrmerge COMMAND tst_ocrmerge)

        if (Tesseract_FOUND)
            qt_add_executable(tst_ocr tests/tst_ocr.cpp src/ocr/ocr.cpp)
            target_include_directories(tst_ocr PRIVATE src)
            target_link_libraries(tst_ocr PRIVATE Qt6::Test Qt6::Gui Tesseract::libtesseract)
            target_compile_definitions(tst_ocr PRIVATE LB_HAVE_OCR
                LB_TESSDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/tessdata")
            set_target_properties(tst_ocr PROPERTIES WIN32_EXECUTABLE FALSE)
            add_test(NAME tst_ocr COMMAND tst_ocr)
        endif()

        qt_add_executable(tst_googletranslate tests/tst_googletranslate.cpp
            src/translation/googletranslate.cpp)
        target_include_directories(tst_googletranslate PRIVATE src)
        target_link_libraries(tst_googletranslate PRIVATE Qt6::Test)
        set_target_properties(tst_googletranslate PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_googletranslate COMMAND tst_googletranslate)

        qt_add_executable(tst_cardstore tests/tst_cardstore.cpp
            src/cardstore.cpp src/cardstore.h src/cardimages.cpp)
        target_include_directories(tst_cardstore PRIVATE src)
        target_link_libraries(tst_cardstore PRIVATE Qt6::Test Qt6::Gui Qt6::Sql Qt6::Qml)
        set_target_properties(tst_cardstore PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_cardstore COMMAND tst_cardstore)

        qt_add_executable(tst_starterdeck tests/tst_starterdeck.cpp src/starterdeck.cpp)
        target_include_directories(tst_starterdeck PRIVATE src)
        target_compile_definitions(tst_starterdeck PRIVATE
            LB_STARTER_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/starter/starter_100.tsv")
        target_link_libraries(tst_starterdeck PRIVATE Qt6::Test)
        set_target_properties(tst_starterdeck PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_starterdeck COMMAND tst_starterdeck)

        qt_add_executable(tst_cardstore_migrate tests/tst_cardstore_migrate.cpp
            src/cardstore.cpp src/cardstore.h src/cardimages.cpp)
        target_include_directories(tst_cardstore_migrate PRIVATE src)
        target_link_libraries(tst_cardstore_migrate PRIVATE Qt6::Test Qt6::Gui Qt6::Sql Qt6::Qml)
        set_target_properties(tst_cardstore_migrate PROPERTIES WIN32_EXECUTABLE FALSE)
        add_test(NAME tst_cardstore_migrate COMMAND tst_cardstore_migrate)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS appLearningBox
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
