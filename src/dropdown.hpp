/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2024  Brian LePage
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

#include <glibmm/refptr.h>
#include <gtkmm/builder.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/listitem.h>
#include <giomm/liststore.h>
#include <optional>
#include <string>
#include <utility>

class Dropdown : public Gtk::DropDown {
  public:
    Dropdown(BaseObjectType* cobj,
             const Glib::RefPtr<Gtk::Builder>& /*builder*/);
    Dropdown(const Dropdown&) = delete;
    Dropdown& operator=(const Dropdown&) = delete;
    ~Dropdown() = default;

    void append(const std::string& option, const std::string& description);
    void remove_all();
    std::optional<std::pair<std::string, std::string>> get_active_values();

  private:
    class ListItemModel : public Glib::Object {
      public:
        std::string m_option;
        std::string m_description;

        static Glib::RefPtr<ListItemModel>
        create(const std::string& option, const std::string& description) {
            return Glib::make_refptr_for_instance<ListItemModel>(
                new ListItemModel(option, description));
        }

      private:
        ListItemModel(const std::string& option, const std::string& description)
            : m_option{option}, m_description{description} {}
    };

    Glib::RefPtr<Gio::ListStore<ListItemModel>> m_list_store;

    static void on_setup_selected(const Glib::RefPtr<Gtk::ListItem>& item);
    static void on_bind_selected(const Glib::RefPtr<Gtk::ListItem>& item);
    static void on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& item);
    void on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& item);
    static void on_unbind_list_item(const Glib::RefPtr<Gtk::ListItem>& item);
    void on_selected_changed(const Glib::RefPtr<Gtk::ListItem>& item);
};
