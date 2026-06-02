#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "TcpClient.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("HyperTicket Admin");
    app.setOrganizationName("HyperTicket");

    TcpClient tcpClient;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("tcpClient", &tcpClient);

    const QUrl url(u"qrc:/HyperTicketAdmin/qml/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
