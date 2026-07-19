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

#include "neonobd_types.hpp"
#include <QPushButton>
#include <QWidget>

class MainWindow;

using neon::ResponseType;

class ConnectButton : public QPushButton {
    Q_OBJECT
  public:
    explicit ConnectButton(QWidget* parent);

  private:
    void user_yes_no_response(const QString& prompt, void* handle);
    void user_text_response(const QString& prompt, void* handle);
    void user_number_response(const QString& prompt, void* handle);
    void send_cancel(void* handle);

    void connect_complete(bool result);
    void user_prompt(const std::string& prompt, ResponseType responseType,
                     void* handle);
  private slots:
    void on_clicked();
};
