#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]) {
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "wayland");
  }
  if (qEnvironmentVariableIsEmpty("QT_IM_MODULE")) {
    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
  }
  if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
  }

  QGuiApplication app(argc, argv);
  app.setOrganizationName("BierKistnRadio");
  app.setApplicationName("BierKistnRadio");

  QQmlApplicationEngine engine;
  engine.loadFromModule("BierKistnRadio", "Main");

  if (engine.rootObjects().isEmpty()) {
    return -1;
  }

  return app.exec();
}
