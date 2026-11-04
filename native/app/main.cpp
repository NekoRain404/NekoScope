#include "HardwareService.h"
#include "Preferences.h"
#include "ReportBuilder.h"
#include "ReportService.h"
#include "core/SensorService.h"
#include "core/TestRunner.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>
#include <QTemporaryDir>
#include <QStyleHints>
#include <QTimer>
#include <memory>

namespace {
void identity()
{
    QCoreApplication::setOrganizationName("NekoScope");
    QCoreApplication::setOrganizationDomain("nekoscope.io");
    QCoreApplication::setApplicationName("NekoScope");
    QCoreApplication::setApplicationVersion("0.2.0");
}
QString graphicsName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software: return QStringLiteral("Software");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL (RHI)");
    case QSGRendererInterface::Vulkan: return QStringLiteral("Vulkan (RHI)");
    case QSGRendererInterface::Metal: return QStringLiteral("Metal (RHI)");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("Direct3D 11 (RHI)");
    default: return QStringLiteral("Unknown");
    }
}
}

int main(int argc, char *argv[])
{
    bool dump = false, software = false;
    for (int i = 1; i < argc; ++i) {
        dump = dump || QByteArray(argv[i]) == "--dump-inventory";
        software = software || QByteArray(argv[i]) == "--software";
    }
    if (dump) {
        QCoreApplication app(argc, argv);
        identity();
        const auto inventory = neko::collectInventory();
        QTextStream(stdout) << neko::ReportBuilder::serialize(
            neko::ReportBuilder::document(inventory.system, inventory.devices, {}, {}, false, {}), "json");
        return inventory.devices.isEmpty() ? 1 : 0;
    }
    if (software) QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    identity();
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.setApplicationDescription("NekoScope — Linux hardware, sensors and diagnostics");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        QCommandLineOption(QStringLiteral("dump-inventory"),
                           QStringLiteral("Print a redacted real hardware snapshot as JSON")),
        QCommandLineOption(QStringLiteral("software"),
                           QStringLiteral("Use Qt Quick software rendering")),
        QCommandLineOption(QStringLiteral("smoke-test"),
                           QStringLiteral("Verify all pages, themes and languages then exit")),
        QCommandLineOption(QStringLiteral("snapshot"),
                           QStringLiteral("Save a window screenshot then exit"),
                           QStringLiteral("path"))});
    parser.process(app);

    const bool smoke = parser.isSet("smoke-test");
    QTemporaryDir verificationSettings;
    if (smoke) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, verificationSettings.path());
    }
    neko::Preferences preferences;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    preferences.setSystemDark(app.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &preferences,
        [&preferences](Qt::ColorScheme scheme) { preferences.setSystemDark(scheme == Qt::ColorScheme::Dark); });
#else
    preferences.setSystemDark(app.palette().color(QPalette::Window).lightness() < 128);
#endif
    neko::HardwareService hardware;
    neko::SensorService sensors;
    neko::TestRunner tests;
    neko::ReportService reports(hardware, sensors, tests);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("preferences", &preferences);
    engine.rootContext()->setContextProperty("hardware", &hardware);
    engine.rootContext()->setContextProperty("sensors", &sensors);
    engine.rootContext()->setContextProperty("testRunner", &tests);
    engine.rootContext()->setContextProperty("reports", &reports);
    QObject::connect(&sensors, &neko::SensorService::temperatureSampled, &tests, &neko::TestRunner::observeTemperature);
    int warnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&warnings](const QList<QQmlError> &errors) { warnings += errors.size(); });
    engine.load(QUrl("qrc:/qt/qml/NekoScope/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) { qCritical("Main.qml must create an ApplicationWindow"); return 2; }
    const auto publishRenderer = [window, &hardware] {
        const auto name = graphicsName(window->rendererInterface()->graphicsApi());
        QMetaObject::invokeMethod(&hardware, [&hardware, name] {
            hardware.setRenderBackend(name);
            qInfo().noquote() << "Qt Quick renderer:" << name;
        }, Qt::QueuedConnection);
    };
    QObject::connect(window, &QQuickWindow::sceneGraphInitialized, &hardware,
                     publishRenderer, Qt::DirectConnection);
    // Some platforms initialise the scene graph during engine.load(); in that
    // case the signal has already fired and the backend would stay pending.
    if (window->isSceneGraphInitialized()) publishRenderer();
    hardware.refresh();
    QTimer sampling;
    sampling.setInterval(1000);
    QObject::connect(&sampling, &QTimer::timeout, &sensors, &neko::SensorService::requestSample);
    sampling.start();
    sensors.requestSample();

    const QString snapshotPath = parser.value("snapshot");
    if (smoke || !snapshotPath.isEmpty()) {
        auto tick = std::make_shared<int>(0);
        auto step = std::make_shared<int>(0);
        auto settle = std::make_shared<int>(0);
        auto verification = std::make_unique<QTimer>();
        verification->setInterval(200);
        QTimer *timer = verification.get();
        QObject::connect(timer, &QTimer::timeout, &app, [&, tick, step, timer] {
            ++*tick;
            if (*tick > 100) { qCritical("Verification timed out"); app.exit(3); return; }
            if (hardware.busy() || hardware.devices().isEmpty() || sensors.readings().isEmpty()
                || hardware.renderBackend() == "initializing") return;
            if (smoke && *step < 28) {
                const int page = *step % 7;
                preferences.setTheme(*step < 14 ? "light" : "dark");
                preferences.setLanguage((*step / 7) % 2 == 0 ? "en" : "zh_CN");
                if (!QMetaObject::invokeMethod(window, "navigateTo", Q_ARG(QVariant, page))) {
                    qCritical("Root navigateTo(index) is missing"); app.exit(4); return;
                }
                if (window->property("currentPage").toInt() != page) {
                    qCritical("Navigation did not update currentPage"); app.exit(4); return;
                }
                ++*step;
                return;
            }
            if (!snapshotPath.isEmpty()) {
                if (smoke && *step == 28) {
                    QMetaObject::invokeMethod(window, "navigateTo", Q_ARG(QVariant, 0));
                    ++*step;
                    *settle = 0;
                    return;
                }
                // Let the page transition settle before grabbing the frame;
                // otherwise the snapshot can capture a fully transparent page.
                if (*settle < 3) {
                    ++*settle;
                    return;
                }
                QDir().mkpath(QFileInfo(snapshotPath).absolutePath());
                if (!window->grabWindow().save(snapshotPath)) {
                    qCritical("Unable to save screenshot"); app.exit(5); return;
                }
            }
            timer->stop();
            qInfo() << "Verification:" << hardware.devices().size() << "devices,"
                    << sensors.readings().size() << "sensor readings," << warnings << "QML warnings";
            app.exit(warnings == 0 ? 0 : 6);
        });
        verification->start();
        const int result = app.exec();
        return result;
    }
    return app.exec();
}
