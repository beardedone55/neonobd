/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2026  Brian LePage
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "connect-button.hpp"
#include "hardware-interface.hpp"
#include "neonobd_types.hpp"
#include <unordered_set>
#include <QPushButton>
#include <QObject>

class MainWindow;

class Home : public QObject {
  Q_OBJECT
  public:
    explicit Home(MainWindow* window);
    void init();
    void enable_all();
    void disable_all();
    void set_connected(bool connected);

  private:
    MainWindow* m_window;
    QPushButton* m_settings_btn;
    ConnectButton* m_connect_btn;
    QPushButton* m_terminal_btn;
    std::unordered_set<QPushButton*> m_enabled_buttons;
    bool m_connected = false;

    void enable_button(QPushButton* button);
    void disable_button(QPushButton* button);

  private slots:
    void settings_clicked();
    void terminal_clicked();
};
