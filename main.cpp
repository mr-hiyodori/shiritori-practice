#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "gamecontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Create game controller
    GameController gameController;
    
    // Load QML engine
    QQmlApplicationEngine engine;
    
    // Expose game controller to QML
    engine.rootContext()->setContextProperty("gameController", &gameController);
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);
    
    if (engine.rootObjects().isEmpty())
        return -1;
    
    return app.exec();
}
