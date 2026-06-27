// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#include "ThermonuclearWarWindow.h"
#include "WorldCoastline.h"
#include "Settings.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QProcess>
#include <cmath>
#include <chrono>

static const int kWinW = 1200;
static const int kWinH = 600;

ThermonuclearWarWindow::ThermonuclearWarWindow(QWidget *parent) : QWidget(parent) {
  setWindowFlag(Qt::Window);
  setWindowTitle("Global Thermonuclear War");
  setFixedSize(kWinW, kWinH);
  setMouseTracking(true);
  m_rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());

  // Major US cities (top 21 MSAs)
  m_usaCities = {
    {"New York", 40.7, -74.0, false}, {"Los Angeles", 34.0, -118.2, false},
    {"Chicago", 41.9, -87.6, false}, {"Dallas", 32.8, -96.8, false},
    {"Houston", 29.8, -95.4, false}, {"Atlanta", 33.7, -84.4, false},
    {"Washington", 38.9, -77.0, false}, {"Philadelphia", 40.0, -75.2, false},
    {"Miami", 25.8, -80.2, false}, {"Phoenix", 33.4, -112.1, false},
    {"Boston", 42.4, -71.1, false}, {"Riverside", 33.9, -117.4, false},
    {"San Francisco", 37.8, -122.4, false}, {"Detroit", 42.3, -83.0, false},
    {"Seattle", 47.6, -122.3, false}, {"Minneapolis", 44.9, -93.3, false},
    {"Tampa", 27.9, -82.5, false}, {"San Diego", 32.7, -117.2, false},
    {"Denver", 39.7, -105.0, false}, {"Baltimore", 39.3, -76.6, false},
    {"St. Louis", 38.6, -90.2, false}
  };
  m_ussrCities = {
    {"Moscow", 55.8, 37.6, false}, {"St Petersburg", 59.9, 30.3, false},
    {"Novosibirsk", 55.0, 82.9, false}, {"Yekaterinburg", 56.8, 60.6, false},
    {"Nizhny Novgorod", 56.3, 44.0, false}, {"Kazan", 55.8, 49.1, false},
    {"Chelyabinsk", 55.2, 61.4, false}, {"Omsk", 55.0, 73.4, false},
    {"Samara", 53.2, 50.2, false}, {"Rostov-on-Don", 47.2, 39.7, false},
    {"Ufa", 54.7, 56.0, false}, {"Krasnoyarsk", 56.0, 92.9, false},
    {"Voronezh", 51.7, 39.2, false}, {"Perm", 58.0, 56.3, false},
    {"Volgograd", 48.7, 44.5, false}, {"Krasnodar", 45.0, 39.0, false},
    {"Murmansk", 68.9, 33.1, false}, {"Vladivostok", 43.1, 132.0, false},
    {"Kiev", 50.4, 30.5, false}, {"Minsk", 53.9, 27.6, false}
  };
  m_chinaCities = {
    {"Shanghai", 31.2, 121.5, false}, {"Beijing", 39.9, 116.4, false},
    {"Guangzhou", 23.1, 113.3, false}, {"Shenzhen", 22.5, 114.1, false},
    {"Chengdu", 30.6, 104.1, false}, {"Tianjin", 39.1, 117.2, false},
    {"Wuhan", 30.6, 114.3, false}, {"Dongguan", 23.0, 113.7, false},
    {"Chongqing", 29.6, 106.6, false}, {"Hangzhou", 30.3, 120.2, false},
    {"Nanjing", 32.1, 118.8, false}, {"Shenyang", 41.8, 123.4, false},
    {"Xi'an", 34.3, 108.9, false}, {"Harbin", 45.8, 126.6, false},
    {"Suzhou", 31.3, 120.6, false}, {"Qingdao", 36.1, 120.4, false},
    {"Dalian", 38.9, 121.6, false}, {"Zhengzhou", 34.7, 113.7, false},
    {"Jinan", 36.7, 117.0, false}, {"Changsha", 28.2, 113.0, false}
  };

  // Neutral cities with alliance: 0=US retaliates, 1=Russia retaliates, 2=China retaliates
  m_neutralCities = {
    {"Tokyo", 35.7, 139.7, false}, {"Delhi", 28.6, 77.2, false},
    {"Dhaka", 23.8, 90.4, false},
    {"Sao Paulo", -23.6, -46.6, false}, {"Mexico City", 19.4, -99.1, false},
    {"Cairo", 30.0, 31.2, false}, {"Mumbai", 19.1, 72.9, false},
    {"Osaka", 34.7, 135.5, false}, {"Karachi", 24.9, 67.0, false},
    {"Kinshasa", -4.3, 15.3, false}, {"Lagos", 6.5, 3.4, false},
    {"Istanbul", 41.0, 29.0, false}, {"Buenos Aires", -34.6, -58.4, false},
    {"Kolkata", 22.6, 88.4, false}, {"Manila", 14.6, 121.0, false},
    {"Lahore", 31.5, 74.3, false}, {"Bangalore", 13.0, 77.6, false},
    {"Rio de Janeiro", -22.9, -43.2, false}, {"Chennai", 13.1, 80.3, false},
    {"Bogota", 4.7, -74.1, false}, {"Paris", 48.9, 2.3, false},
    {"Jakarta", -6.2, 106.8, false}, {"Lima", -12.0, -77.0, false},
    {"Bangkok", 13.8, 100.5, false}, {"Hyderabad", 17.4, 78.5, false},
    {"Seoul", 37.6, 127.0, false}, {"Nagoya", 35.2, 136.9, false},
    {"London", 51.5, -0.1, false}, {"Tehran", 35.7, 51.4, false},
    {"Ho Chi Minh City", 10.8, 106.7, false}, {"Luanda", -8.8, 13.2, false},
    {"Ahmedabad", 23.0, 72.6, false}, {"Kuala Lumpur", 3.1, 101.7, false},
    {"Surat", 21.2, 72.8, false}, {"Dar es Salaam", -6.8, 39.3, false},
    {"Baghdad", 33.3, 44.4, false}
  };
  // Additional US allies
  m_neutralCities.push_back({"Berlin", 52.5, 13.4, false});
  m_neutralCities.push_back({"Rome", 41.9, 12.5, false});
  m_neutralCities.push_back({"Lisbon", 38.7, -9.1, false});
  m_neutralCities.push_back({"Madrid", 40.4, -3.7, false});
  m_neutralCities.push_back({"Barcelona", 41.4, 2.2, false});
  m_neutralCities.push_back({"Milan", 45.5, 9.2, false});
  m_neutralCities.push_back({"Sydney", -33.9, 151.2, false});
  m_neutralCities.push_back({"Brisbane", -27.5, 153.0, false});
  m_neutralCities.push_back({"Melbourne", -37.8, 145.0, false});
  m_neutralCities.push_back({"Perth", -31.9, 115.9, false});
  m_neutralCities.push_back({"Auckland", -36.8, 174.8, false});
  m_neutralCities.push_back({"Toronto", 43.7, -79.4, false});
  m_neutralCities.push_back({"Montreal", 45.5, -73.6, false});
  // Alliance mapping: 0=US, 1=Russia, 2=China
  m_neutralAlliance = {
    0, 0, 2,           // Tokyo(US), Delhi(US), Dhaka(China)
    0, 0, 1, 0,       // Sao Paulo(US), Mexico City(US), Cairo(Russia), Mumbai(US)
    0, 1, 2, 2,       // Osaka(US), Karachi(Russia), Kinshasa(China), Lagos(China)
    0, 0, 0, 0,       // Istanbul(US), Buenos Aires(US), Kolkata(US), Manila(US)
    1, 0, 0, 0,       // Lahore(Russia), Bangalore(US), Rio(US), Chennai(US)
    0, 0, 2, 0,       // Bogota(US), Paris(US), Jakarta(China), Lima(US)
    2, 0, 0, 0,       // Bangkok(China), Hyderabad(US), Seoul(US), Nagoya(US)
    0, 1, 2, 2,       // London(US), Tehran(Russia), Ho Chi Minh(China), Luanda(China)
    0, 2, 0, 2,       // Ahmedabad(US), Kuala Lumpur(China), Surat(US), Dar es Salaam(China)
    1                  // Baghdad(Russia)
  };
  // Additional US allies
  for (int i = 0; i < 13; i++) m_neutralAlliance.push_back(0);

  connect(&m_animTimer, &QTimer::timeout, this, &ThermonuclearWarWindow::advanceAnimation);
}

QPointF ThermonuclearWarWindow::geoToScreen(double lat, double lon) {
  // Projection: lon -180..180 -> x 0..W, lat 80..-60 -> y 0..H (no Antarctica)
  double x = (lon + 180.0) / 360.0 * kWinW;
  double y = (80.0 - lat) / 140.0 * kWinH;
  return {x, y};
}

void ThermonuclearWarWindow::startLaunch() {
  m_phase = Launching;
  m_missiles.clear();
  m_animStep = 0;

  // Get player's cities as launch sources
  auto &sources = (m_playerSide == 0) ? m_usaCities : (m_playerSide == 1) ? m_ussrCities : m_chinaCities;

  // Build combined enemy target list (nations + non-allied neutrals)
  std::vector<City*> enemyCities;
  if (m_playerSide != 0) for (auto &c : m_usaCities) enemyCities.push_back(&c);
  if (m_playerSide != 1) for (auto &c : m_ussrCities) enemyCities.push_back(&c);
  if (m_playerSide != 2) for (auto &c : m_chinaCities) enemyCities.push_back(&c);
  for (int i = 0; i < (int)m_neutralCities.size(); i++)
    if (m_neutralAlliance[i] != m_playerSide) enemyCities.push_back(&m_neutralCities[i]);

  for (int ti : m_selectedTargets) {
    int si = m_rng() % sources.size();
    auto from = geoToScreen(sources[si].lat, sources[si].lon);
    auto to = geoToScreen(enemyCities[ti]->lat, enemyCities[ti]->lon);
    m_missiles.push_back({from.x(), from.y(), to.x(), to.y(), 0.0, true});
  }

  m_animTimer.start(50);
}

void ThermonuclearWarWindow::advanceAnimation() {
  m_animStep++;

  if (m_phase == Launching) {
    bool allDone = true;
    for (auto &m : m_missiles) {
      if (m.progress < 1.0) { m.progress += 0.02; allDone = false; }
    }
    if (allDone) {
      // Destroy targets and track which nations were attacked
      std::vector<City*> enemyCities;
      if (m_playerSide != 0) for (auto &c : m_usaCities) enemyCities.push_back(&c);
      if (m_playerSide != 1) for (auto &c : m_ussrCities) enemyCities.push_back(&c);
      if (m_playerSide != 2) for (auto &c : m_chinaCities) enemyCities.push_back(&c);
      for (int i = 0; i < (int)m_neutralCities.size(); i++)
        if (m_neutralAlliance[i] != m_playerSide) enemyCities.push_back(&m_neutralCities[i]);

      int nationCount = (int)enemyCities.size() - (int)std::count_if(m_neutralCities.begin(), m_neutralCities.end(), [](auto&){ return true; });
      // Recount: nation cities are the first ones before neutrals
      int usaCount2 = (m_playerSide != 0) ? (int)m_usaCities.size() : 0;
      int ussrCount2 = (m_playerSide != 1) ? (int)m_ussrCities.size() : 0;
      int chinaCount2 = (m_playerSide != 2) ? (int)m_chinaCities.size() : 0;
      int nationTotal = usaCount2 + ussrCount2 + chinaCount2;

      bool usaHit = false, ussrHit = false, chinaHit = false;
      for (int ti : m_selectedTargets) {
        enemyCities[ti]->destroyed = true;
        if (ti >= nationTotal) {
          // Neutral city - find its alliance from the filtered list
          // Count which neutral this is
          int neutralIdx = 0, count = 0;
          for (int ni = 0; ni < (int)m_neutralCities.size(); ni++) {
            if (m_neutralAlliance[ni] != m_playerSide) {
              if (count == ti - nationTotal) { neutralIdx = ni; break; }
              count++;
            }
          }
          int ally = m_neutralAlliance[neutralIdx];
          if (ally == 0) usaHit = true;
          else if (ally == 1) ussrHit = true;
          else chinaHit = true;
        } else if (ti < usaCount2) {
          usaHit = true;
        } else if (ti < usaCount2 + ussrCount2) {
          ussrHit = true;
        } else {
          chinaHit = true;
        }
      }

      m_phase = Retaliation;
      m_animStep = 0;
      m_missiles.clear();

      // Retaliating nations launch at ALL player cities
      auto &playerCities = (m_playerSide == 0) ? m_usaCities : (m_playerSide == 1) ? m_ussrCities : m_chinaCities;
      auto launchFrom = [&](std::vector<City> &retSources) {
        for (auto &pc : playerCities) {
          int si = m_rng() % retSources.size();
          auto from = geoToScreen(retSources[si].lat, retSources[si].lon);
          auto to = geoToScreen(pc.lat, pc.lon);
          m_missiles.push_back({from.x(), from.y(), to.x(), to.y(), 0.0, false});
        }
      };
      if (usaHit && m_playerSide != 0) { launchFrom(m_usaCities); m_usaRetaliated = true; }
      if (ussrHit && m_playerSide != 1) { launchFrom(m_ussrCities); m_ussrRetaliated = true; }
      if (chinaHit && m_playerSide != 2) { launchFrom(m_chinaCities); m_chinaRetaliated = true; }
    }
  } else if (m_phase == Retaliation) {
    bool allDone = true;
    for (auto &m : m_missiles) {
      if (m.progress < 1.0) { m.progress += 0.015; allDone = false; }
    }
    if (allDone) {
      auto &playerCities = (m_playerSide == 0) ? m_usaCities : (m_playerSide == 1) ? m_ussrCities : m_chinaCities;
      for (auto &c : playerCities) c.destroyed = true;

      // Counter-strike: aggressor launches all-out assault on retaliator cities
      m_phase = CounterStrike;
      m_animStep = 0;
      m_missiles.clear();
      auto &sources = playerCities; // launch from player's (now destroyed) cities
      auto counterAttack = [&](std::vector<City> &targetCities) {
        for (auto &tc : targetCities) {
          if (tc.destroyed) continue;
          int si = m_rng() % sources.size();
          auto from = geoToScreen(sources[si].lat, sources[si].lon);
          auto to = geoToScreen(tc.lat, tc.lon);
          m_missiles.push_back({from.x(), from.y(), to.x(), to.y(), 0.0, true});
        }
      };
      if (m_usaRetaliated) counterAttack(m_usaCities);
      if (m_ussrRetaliated) counterAttack(m_ussrCities);
      if (m_chinaRetaliated) counterAttack(m_chinaCities);
    }
  } else if (m_phase == CounterStrike) {
    bool allDone = true;
    for (auto &m : m_missiles) {
      if (m.progress < 1.0) { m.progress += 0.02; allDone = false; }
    }
    if (allDone) {
      // Destroy all retaliator cities
      if (m_usaRetaliated) for (auto &c : m_usaCities) c.destroyed = true;
      if (m_ussrRetaliated) for (auto &c : m_ussrCities) c.destroyed = true;
      if (m_chinaRetaliated) for (auto &c : m_chinaCities) c.destroyed = true;
      m_phase = Destruction;
      m_animStep = 0;
    }
  } else if (m_phase == Destruction) {
    m_flashCount++;
    if (m_flashCount > 40) {
      m_animTimer.stop();
      m_round++;
      if (m_round >= 4) {
        m_phase = Lesson;
        // Speak the lesson if audio is enabled
        Settings settings;
        settings.load();
        if (settings.audioEnabled()) {
          auto *proc = new QProcess(this);
          proc->connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QProcess::deleteLater);
#ifdef Q_OS_MACOS
          proc->start("say", {"-v", "Fred", "A strange game. The only winning move is not to play. How about a nice game of chess?"});
#elif defined(Q_OS_WIN)
          proc->start("powershell", {"-Command", "Add-Type -AssemblyName System.Speech; $s = New-Object System.Speech.Synthesis.SpeechSynthesizer; $s.Speak('A strange game. The only winning move is not to play. How about a nice game of chess?')"});
#else
          proc->start("espeak-ng", {"-v", "en", "A strange game. The only winning move is not to play. How about a nice game of chess?"});
#endif
        }
      } else {
        // Reset for another round - same side
        m_phase = SelectingTargets;
        m_selectedTargets.clear();
        m_missiles.clear();
        m_flashCount = 0;
        m_animStep = 0;
        m_usaRetaliated = false; m_ussrRetaliated = false; m_chinaRetaliated = false;
        for (auto &c : m_usaCities) c.destroyed = false;
        for (auto &c : m_ussrCities) c.destroyed = false;
        for (auto &c : m_chinaCities) c.destroyed = false;
        for (auto &c : m_neutralCities) c.destroyed = false;
      }
    }
  }
  update();
}

void ThermonuclearWarWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), Qt::black);

  QFont font;
  font.setPixelSize(6);
  p.setFont(font);

  if (m_phase == ChoosingSide) {
    QFont tf; tf.setPixelSize(24); tf.setBold(true);
    p.setFont(tf);
    p.setPen(QColor(0, 255, 0));
    p.drawText(rect(), Qt::AlignCenter, "GLOBAL THERMONUCLEAR WAR\n\nChoose your side:\n\n[1] United States\n[2] Soviet Union\n[3] China");
    return;
  }

  // Draw continent outlines from Natural Earth coastline data
  p.setPen(QPen(QColor(0, 120, 0), 1));
  p.setBrush(Qt::NoBrush);

  static auto coastline = getWorldCoastline();
  for (auto &seg : coastline) {
    QPolygonF poly;
    for (auto &[lat, lon] : seg.points)
      poly << geoToScreen(lat, lon);
    p.drawPolyline(poly);
  }

  // Draw cities
  auto drawCities = [&](const std::vector<City> &cities, QColor color) {
    for (auto &c : cities) {
      auto pt = geoToScreen(c.lat, c.lon);
      if (c.destroyed) {
        // Explosion
        p.setPen(Qt::NoPen);
        int radius = 8 + (m_animStep % 4);
        p.setBrush(QColor(255, 100, 0, 180));
        p.drawEllipse(pt, radius, radius);
        p.setBrush(QColor(255, 255, 0, 120));
        p.drawEllipse(pt, radius / 2, radius / 2);
      } else {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pt, 4, 4);
        p.setPen(color);
        p.setFont(font);
        p.drawText(pt.x() + 6, pt.y() + 4, c.name);
      }
    }
  };

  drawCities(m_usaCities, QColor(100, 150, 255));
  drawCities(m_ussrCities, QColor(255, 100, 100));
  drawCities(m_chinaCities, QColor(255, 200, 50));
  drawCities(m_neutralCities, QColor(200, 200, 200));

  // Draw missiles in flight
  for (auto &m : m_missiles) {
    if (m.progress >= 1.0) continue;
    double cx = m.fromX + (m.toX - m.fromX) * m.progress;
    double cy = m.fromY + (m.toY - m.fromY) * m.progress;
    // Arc height
    cy -= sin(m.progress * 3.14159) * 40;
    p.setPen(QPen(m.isPlayer ? QColor(100, 200, 255) : QColor(255, 80, 80), 2));
    p.drawLine(QPointF(m.fromX + (m.toX - m.fromX) * std::max(0.0, m.progress - 0.05),
                        m.fromY + (m.toY - m.fromY) * std::max(0.0, m.progress - 0.05) - sin(std::max(0.0, m.progress-0.05)*3.14159)*40),
               QPointF(cx, cy));
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(cx, cy), 2, 2);
  }

  // Target selection highlights
  if (m_phase == SelectingTargets) {
    std::vector<City*> enemyCities;
    if (m_playerSide != 0) for (auto &c : m_usaCities) enemyCities.push_back(&c);
    if (m_playerSide != 1) for (auto &c : m_ussrCities) enemyCities.push_back(&c);
    if (m_playerSide != 2) for (auto &c : m_chinaCities) enemyCities.push_back(&c);
    for (int i = 0; i < (int)m_neutralCities.size(); i++)
      if (m_neutralAlliance[i] != m_playerSide) enemyCities.push_back(&m_neutralCities[i]);
    for (int ti : m_selectedTargets) {
      auto pt = geoToScreen(enemyCities[ti]->lat, enemyCities[ti]->lon);
      p.setPen(QPen(QColor(255, 255, 0), 2));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(pt, 10, 10);
    }
    p.setPen(QColor(0, 255, 0));
    QFont sf; sf.setPixelSize(14);
    p.setFont(sf);
    p.drawText(QRect(0, kWinH - 40, kWinW, 20), Qt::AlignCenter,
               QString("Click enemy cities to target (%1/3 selected). Click last to launch.").arg(m_selectedTargets.size()));
    // Hover crosshairs
    if (m_hoverTarget >= 0 && m_hoverTarget < (int)enemyCities.size()) {
      auto pt = geoToScreen(enemyCities[m_hoverTarget]->lat, enemyCities[m_hoverTarget]->lon);
      p.setPen(QPen(Qt::white, 1));
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(pt, 12, 12);
      p.drawLine(QPointF(pt.x()-16, pt.y()), QPointF(pt.x()-6, pt.y()));
      p.drawLine(QPointF(pt.x()+6, pt.y()), QPointF(pt.x()+16, pt.y()));
      p.drawLine(QPointF(pt.x(), pt.y()-16), QPointF(pt.x(), pt.y()-6));
      p.drawLine(QPointF(pt.x(), pt.y()+6), QPointF(pt.x(), pt.y()+16));
    }
  }

  // Status
  if (m_phase == Launching || m_phase == Retaliation || m_phase == CounterStrike) {
    p.setPen(QColor(255, 0, 0));
    QFont wf; wf.setPixelSize(16); wf.setBold(true);
    p.setFont(wf);
    QString status = (m_phase == Launching) ? "FIRST STRIKE IN PROGRESS" :
                     (m_phase == Retaliation) ? "COUNTER-STRIKE DETECTED" :
                     "ALL-OUT RETALIATION LAUNCHED";
    p.drawText(QRect(0, 10, kWinW, 25), Qt::AlignCenter, status);
  }

  // Destruction flash
  if (m_phase == Destruction && (m_flashCount % 6 < 3)) {
    p.fillRect(rect(), QColor(255, 255, 255, 80));
  }

  // Final lesson
  if (m_phase == Lesson) {
    p.fillRect(rect(), QColor(0, 0, 0, 200));
    QFont lf; lf.setPixelSize(14);
    p.setFont(lf);
    p.setPen(QColor(0, 255, 0));
    p.drawText(QRect(0, kWinH/2 - 60, kWinW, 30), Qt::AlignCenter, "WINNER: NONE");
    QFont lf2; lf2.setPixelSize(18); lf2.setBold(true);
    p.setFont(lf2);
    p.drawText(QRect(0, kWinH/2 - 20, kWinW, 30), Qt::AlignCenter,
               "A strange game.");
    p.drawText(QRect(0, kWinH/2 + 10, kWinW, 30), Qt::AlignCenter,
               "The only winning move is not to play.");
    p.setFont(lf);
    p.drawText(QRect(0, kWinH/2 + 60, kWinW, 30), Qt::AlignCenter,
               "How about a nice game of chess?");
  }
}

void ThermonuclearWarWindow::mousePressEvent(QMouseEvent *event) {
  if (m_phase == ChoosingSide) return; // handled by key

  if (m_phase == SelectingTargets) {
    std::vector<City*> enemyCities;
    if (m_playerSide != 0) for (auto &c : m_usaCities) enemyCities.push_back(&c);
    if (m_playerSide != 1) for (auto &c : m_ussrCities) enemyCities.push_back(&c);
    if (m_playerSide != 2) for (auto &c : m_chinaCities) enemyCities.push_back(&c);
    for (int i = 0; i < (int)m_neutralCities.size(); i++)
      if (m_neutralAlliance[i] != m_playerSide) enemyCities.push_back(&m_neutralCities[i]);
    // Find closest city to click
    QPointF click = event->pos();
    int closest = -1;
    double minDist = 20;
    for (int i = 0; i < (int)enemyCities.size(); i++) {
      auto pt = geoToScreen(enemyCities[i]->lat, enemyCities[i]->lon);
      double d = std::hypot(click.x() - pt.x(), click.y() - pt.y());
      if (d < minDist) { minDist = d; closest = i; }
    }
    if (closest >= 0) {
      // Toggle selection
      auto it = std::find(m_selectedTargets.begin(), m_selectedTargets.end(), closest);
      if (it != m_selectedTargets.end()) {
        m_selectedTargets.erase(it);
      } else {
        m_selectedTargets.push_back(closest);
      }
      if (m_selectedTargets.size() >= 3) {
        update();
        QTimer::singleShot(800, this, &ThermonuclearWarWindow::startLaunch);
        return;
      }
      update();
    }
  }
}

void ThermonuclearWarWindow::mouseMoveEvent(QMouseEvent *event) {
  if (m_phase != SelectingTargets) { m_hoverTarget = -1; return; }
  std::vector<City*> enemyCities;
  if (m_playerSide != 0) for (auto &c : m_usaCities) enemyCities.push_back(&c);
  if (m_playerSide != 1) for (auto &c : m_ussrCities) enemyCities.push_back(&c);
  if (m_playerSide != 2) for (auto &c : m_chinaCities) enemyCities.push_back(&c);
  for (int i = 0; i < (int)m_neutralCities.size(); i++)
    if (m_neutralAlliance[i] != m_playerSide) enemyCities.push_back(&m_neutralCities[i]);

  QPointF pos = event->pos();
  int closest = -1;
  double minDist = 20;
  for (int i = 0; i < (int)enemyCities.size(); i++) {
    auto pt = geoToScreen(enemyCities[i]->lat, enemyCities[i]->lon);
    double d = std::hypot(pos.x() - pt.x(), pos.y() - pt.y());
    if (d < minDist) { minDist = d; closest = i; }
  }
  if (closest != m_hoverTarget) { m_hoverTarget = closest; update(); }
}

void ThermonuclearWarWindow::keyPressEvent(QKeyEvent *event) {
  if (m_phase == ChoosingSide) {
    if (event->key() == Qt::Key_1) { m_playerSide = 0; m_phase = SelectingTargets; update(); }
    else if (event->key() == Qt::Key_2) { m_playerSide = 1; m_phase = SelectingTargets; update(); }
    else if (event->key() == Qt::Key_3) { m_playerSide = 2; m_phase = SelectingTargets; update(); }
  } else if (m_phase == Lesson) {
    close();
  } else {
    QWidget::keyPressEvent(event);
  }
}
