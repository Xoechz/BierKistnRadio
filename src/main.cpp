#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "wayland");

  QGuiApplication app(argc, argv);
  app.setOrganizationName("BierKistnRadio");
  app.setApplicationName("BierKistnRadio");

  QQmlApplicationEngine engine;
  engine.loadFromModule("BierKistnRadio", "Main");

  if (engine.rootObjects().isEmpty())
    return -1;

  return app.exec();
}
