// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include <QApplication>
#include <QFileOpenEvent>
#include <QStyleFactory>
#include <QPalette>
#include <QStyle>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <thread>
#include "MainWindow.h"
#include "SplashScreen.h"
#include "Settings.h"
#include "LicenseValidator.h"
#include "CheckersEndgame.h"

class Application : public QApplication {
public:
  using QApplication::QApplication;
  MainWindow *mainWindow = nullptr;

protected:
  bool event(QEvent *event) override {
    return QApplication::event(event);
  }

public:
  QString m_pendingFile;
};

static void applyAppTheme(const Settings &settings) {
  QString theme = settings.theme();
  if (theme == "Dark") {
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor("#2b2b2b"));
    p.setColor(QPalette::WindowText, QColor("#e0e0e0"));
    p.setColor(QPalette::Base, QColor("#1e1e1e"));
    p.setColor(QPalette::AlternateBase, QColor("#333333"));
    p.setColor(QPalette::Text, QColor("#e0e0e0"));
    p.setColor(QPalette::Button, QColor("#3c3c3c"));
    p.setColor(QPalette::ButtonText, QColor("#e0e0e0"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Highlight, QColor("#555555"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::ToolTipBase, QColor("#3c3c3c"));
    p.setColor(QPalette::ToolTipText, QColor("#e0e0e0"));
    p.setColor(QPalette::PlaceholderText, QColor("#888888"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#666666"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#666666"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#666666"));
    qApp->setPalette(p);
  } else if (theme == "Light") {
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor("#f5f5f5"));
    p.setColor(QPalette::WindowText, QColor("#1a1a1a"));
    p.setColor(QPalette::Base, QColor("#ffffff"));
    p.setColor(QPalette::Text, QColor("#1a1a1a"));
    p.setColor(QPalette::Button, QColor("#e8e8e8"));
    p.setColor(QPalette::ButtonText, QColor("#1a1a1a"));
    p.setColor(QPalette::Highlight, QColor("#3399ff"));
    p.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#aaaaaa"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#aaaaaa"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#aaaaaa"));
    qApp->setPalette(p);
  } else {
    // System default
    qApp->setStyle(QStyleFactory::create("macOS"));
    qApp->setPalette(qApp->style()->standardPalette());
    qApp->setStyleSheet(QString());
  }
}

int main(int argc, char *argv[]) {
  Application app(argc, argv);
  app.setApplicationName("Joshua");
  app.setApplicationVersion(APP_VERSION);
  app.setOrganizationName("Richard Lesh");
  app.setWindowIcon(QIcon(":/icons/app_icon.png"));

  Settings settings;
  settings.load();

  applyAppTheme(settings);

  LicenseValidator validator;
  bool licensed = validator.isValid(settings.userName(), settings.licenseKey());

  if (!licensed) {
    SplashScreen splash;
    splash.exec();
  }

  MainWindow mainWindow(settings, licensed);
  app.mainWindow = &mainWindow;
  mainWindow.show();

  // Build endgame database in background thread
  std::thread([&mainWindow]() {
    // If Chinook DB is already cached, load it then notify
    if (CheckersEndgame::instance().chinookCached()) {
      CheckersEndgame::instance().loadChinook();
    }
  }).detach();

  // After window is shown, prompt user to download Chinook DB if not already cached
  QTimer::singleShot(500, &mainWindow, [&mainWindow]() {
    if (CheckersEndgame::instance().isChinookReady()) return;
    if (CheckersEndgame::instance().chinookCached()) return;

    auto reply = QMessageBox::question(&mainWindow,
      "Download Endgame Database",
      "Would you like to download the Chinook 6-piece checkers endgame database?\n\n"
      "This enables perfect play in all positions with 6 or fewer pieces.\n"
      "Download size: ~450 MB\n\n"
      "The download will happen in the background.",
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply == QMessageBox::Yes) {
      std::thread([&mainWindow]() {
        CheckersEndgame::instance().loadChinook();
        QMetaObject::invokeMethod(&mainWindow, [&mainWindow]() {
          if (CheckersEndgame::instance().isChinookReady()) {
            QMessageBox::information(&mainWindow,
              "Endgame Database Ready",
              "The Chinook 6-piece checkers endgame database has been downloaded and is now active.\n\n"
              "The checkers engine now has perfect play for all positions with 6 or fewer pieces.");
          } else {
            QMessageBox::warning(&mainWindow,
              "Download Failed",
              "Failed to download the endgame database.\n"
              "Please check your internet connection and try again later.");
          }
        }, Qt::QueuedConnection);
      }).detach();
    }
  });

  return app.exec();
}
