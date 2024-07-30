/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2023  Brian LePage
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

#include "neonobd_types.h"
#include <glibmm/ustring.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <sigc++/connection.h>

class MainWindow;

using neon::ResponseType;

class ConnectButton : public Gtk::Button {
  public:
    ConnectButton(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& ui);

  private:
    MainWindow* window;
    sigc::connection user_prompt_connection;
    sigc::connection connect_complete_connection;
    Glib::RefPtr<void> user_prompt_handle;

    void clicked();
    void connect_complete(bool result);
    void user_prompt(Glib::ustring prompt, ResponseType responseType,
                     Glib::RefPtr<void>);

    void user_yes_no_response(int responseCode);

    Glib::ustring get_user_input(int responseCode, const char* widget);
    void user_text_response(int responseCode);
    void user_number_response(int responseCode);
    void user_none_response(int responseCode);
    void send_cancel();
};
