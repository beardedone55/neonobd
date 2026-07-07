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

#include <mutex>
#include <thread>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>

class MainWindow;

class Terminal : public QObject {
  Q_OBJECT
  public:
    explicit Terminal(MainWindow* window);
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    ~Terminal();
    void init();

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    MainWindow* m_window;
    QPushButton* m_home_button;
    QPlainTextEdit* m_terminal;
    int m_input_begin = 0;
    std::unique_ptr<std::thread> m_reader_thread;
    volatile bool m_stop_reader = false;
    volatile bool m_reader_stopped = true;
    std::string m_read_buffer;
    std::mutex m_read_buffer_mutex;

    void read_data();
    void start_reader_thread();
    void text_entered();
    void reset_input_begin();

  signals:
    void read_data_available();

  private slots:
    void on_show();
    void home_clicked();
    void reset_cursor();
    void reader_notification();
};
