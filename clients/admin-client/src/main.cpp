#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLibraryInfo>
#include <QDir>
#include "TcpClient.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("HyperTicket Admin");
    app.setOrganizationName("HyperTicket");

    TcpClient tcpClient;

    QQmlApplicationEngine engine;

    // 1. 系统 Qt6 QML 路径（QtQuick、QtQuick.Controls 等内置模块）
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));

    // 2. 构建目录下的 qml/（md3.Core 构建输出在 build/qml/md3/Core/）
    //    可执行文件在 build/bin/，所以上一级的 qml/ 即为 build/qml/
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/../qml");

    engine.rootContext()->setContextProperty("tcpClient", &tcpClient);

    const QUrl url(u"qrc:/HyperTicketAdmin/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
