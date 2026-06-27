// Copyright (c) 2026, Richard Lesh. All Rights Reserved.
// License: GPL v3.0

#ifndef THERMONUCLEARWARWINDOW_H
#define THERMONUCLEARWARWINDOW_H

#include <QWidget>
#include <QTimer>
#include <vector>
#include <random>

class ThermonuclearWarWindow : public QWidget {
  Q_OBJECT
public:
  explicit ThermonuclearWarWindow(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  struct City { QString name; double lat, lon; bool destroyed; };
  struct Missile { double fromX, fromY, toX, toY; double progress; bool isPlayer; };

  enum Phase { ChoosingSide, SelectingTargets, Launching, Retaliation, CounterStrike, Destruction, Lesson };

  Phase m_phase = ChoosingSide;
  int m_playerSide = 0; // 0=USA, 1=USSR, 2=China
  std::vector<City> m_usaCities;
  std::vector<City> m_ussrCities;
  std::vector<City> m_chinaCities;
  std::vector<int> m_selectedTargets;
  std::vector<int> m_selectedTargetsChina; // targets in China
  std::vector<Missile> m_missiles;
  QTimer m_animTimer;
  int m_animStep = 0;
  int m_flashCount = 0;
  int m_round = 0;
  bool m_usaRetaliated = false, m_ussrRetaliated = false, m_chinaRetaliated = false;
  std::mt19937 m_rng;

  QPointF geoToScreen(double lat, double lon);
  void startLaunch();
  void advanceAnimation();
};

#endif // THERMONUCLEARWARWINDOW_H
