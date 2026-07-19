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

#include "bluetooth-serial-port.hpp"
#include "neonobd_types.hpp"
#include "serial-port.hpp"
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QString>
#include <QWidget>

class MainWindow;

using neon::InterfaceType;

class Settings : public QObject {
    Q_OBJECT
  public:
    explicit Settings(MainWindow* window);
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    ~Settings();
    void init();
    QString get_selected_device();

  private:
    QPushButton* m_home_button = nullptr;
    MainWindow* m_window = nullptr;
    QRadioButton* m_bluetooth_rb = nullptr;
    QRadioButton* m_serial_rb = nullptr;
    QWidget* m_bt_grid = nullptr;
    QLabel* m_host_label = nullptr;
    QComboBox* m_bt_host_dropdown = nullptr;
    QLabel* m_bt_device_label = nullptr;
    QComboBox* m_bt_device_dropdown = nullptr;
    QLabel* m_bt_scan_label = nullptr;
    QProgressBar* m_bt_scan_progress = nullptr;
    QPushButton* m_bt_device_scan = nullptr;
    QWidget* m_serial_grid = nullptr;
    QComboBox* m_serial_device_dropdown = nullptr;
    QComboBox* m_serial_baudrate_dropdown = nullptr;
    BluetoothSerialPort& m_bt_hardware_interface;
    SerialPort& m_serial_hardware_interface;
    QSettings m_settings;
    InterfaceType m_iftype = neon::BLUETOOTH_IF;

  private slots:
    void home_clicked();
    void on_show();
    void select_serial();
    void select_bluetooth();
    void select_bluetooth_controller(int index);
    void select_bluetooth_device(int index);
    void scan_bluetooth();
    void select_serial_device(int index);
    void select_serial_baudrate(int index);

  private:
    void scan_complete();
    void update_scan_progress(int percent_complete);
    static void populate_dropdown(const std::vector<std::string>& values,
                                  QComboBox* dropdown,
                                  const QString& default_value);
    static bool valid_dropdown_index(int index, QComboBox* dropdown);
    void add_device(const QString& name, const QString& address);
};
