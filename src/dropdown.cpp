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

#include "dropdown.hpp"
#include <giomm/liststore.h>
#include <glibmm/refptr.h>
#include <glibmm/utility.h>

// gtk.h provides GTK_INVALID_LIST_POSITION indirectly.
// direct inclusion of the header that provides it is not allowed.
#include <gtk/gtk.h> //NOLINT(misc-include-cleaner)
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/enums.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/listitem.h>
#include <gtkmm/object.h>
#include <gtkmm/signallistitemfactory.h>
#include <memory>
#include <optional>
#include <sigc++/adaptors/bind.h>
#include <sigc++/connection.h>
#include <sigc++/functors/mem_fun.h>
#include <string>
#include <utility>

Dropdown::Dropdown(BaseObjectType* cobj,
                   const Glib::RefPtr<Gtk::Builder>& /*builder*/)
    : Gtk::DropDown(cobj) {
    m_list_store = Gio::ListStore<ListItemModel>::create();
    set_model(m_list_store);

    // Factory for displaying selected item:
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::mem_fun(*this, &Dropdown::on_setup_selected));
    factory->signal_bind().connect(
        sigc::mem_fun(*this, &Dropdown::on_bind_selected));
    set_factory(factory);

    // Factory for displaying list of items:
    factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::mem_fun(*this, &Dropdown::on_setup_list_item));
    factory->signal_bind().connect(
        sigc::mem_fun(*this, &Dropdown::on_bind_list_item));
    factory->signal_unbind().connect(
        sigc::mem_fun(*this, &Dropdown::on_unbind_list_item));
    set_list_factory(factory);
}

void Dropdown::append(const std::string& option,
                      const std::string& description) {
    m_list_store->append(ListItemModel::create(option, description));
}

void Dropdown::remove_all() { m_list_store->remove_all(); }

std::optional<std::pair<std::string, std::string>>
Dropdown::get_active_values() {
    const auto selected = get_selected();

    // GTK_INVALID_LIST_POSITION is provided by gtk.h indirectly.
    // NOLINTNEXTLINE(misc-include-cleaner)
    if (selected == GTK_INVALID_LIST_POSITION) {
        return {};
    }
    const auto selected_item = m_list_store->get_item(selected);
    return std::make_pair(selected_item->m_option,
                          selected_item->m_description);
}

// These functions are converted to slots, and we use sigc::mem_fun
// to automatically disconnect connections when the dropdown is
// destroyed.  We cannot declare any of them static.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
void Dropdown::on_setup_selected(const Glib::RefPtr<Gtk::ListItem>& item) {
    item->set_child(*Gtk::make_managed<Gtk::Label>());
}

void Dropdown::on_bind_selected(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto selection = std::dynamic_pointer_cast<ListItemModel>(item->get_item());
    if (!selection) {
        return;
    }
    auto* label = dynamic_cast<Gtk::Label*>(item->get_child());
    if (label == nullptr) {
        return;
    }
    label->set_text(selection->m_option);
}

void Dropdown::on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    constexpr int BOX_WIDTH = 10;
    constexpr int BOX_HEIGHT = 2;
    auto* hbox =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, BOX_WIDTH);
    auto* vbox =
        Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, BOX_HEIGHT);
    hbox->append(*vbox);
    auto* option = Gtk::make_managed<Gtk::Label>();
    option->set_xalign(0.0);
    vbox->append(*option);
    auto* description = Gtk::make_managed<Gtk::Label>();
    description->set_xalign(0.0);
    description->add_css_class("dim-label");
    vbox->append(*description);
    auto* checkmark = Gtk::make_managed<Gtk::Image>();
    checkmark->set_from_icon_name("object-select-symbolic");
    checkmark->set_visible(false);
    hbox->append(*checkmark);
    item->set_child(*hbox);
}

void Dropdown::on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto selection = std::dynamic_pointer_cast<ListItemModel>(item->get_item());
    if (!selection) {
        return;
    }
    auto* hbox = dynamic_cast<Gtk::Box*>(item->get_child());
    if (hbox == nullptr) {
        return;
    }
    auto* vbox = dynamic_cast<Gtk::Box*>(hbox->get_first_child());
    if (vbox == nullptr) {
        return;
    }
    auto* option = dynamic_cast<Gtk::Label*>(vbox->get_first_child());
    if (option == nullptr) {
        return;
    }
    option->set_text(selection->m_option);
    auto* description = dynamic_cast<Gtk::Label*>(option->get_next_sibling());
    if (description == nullptr) {
        return;
    }
    description->set_text(selection->m_description);
    auto connection = property_selected_item().signal_changed().connect(
        sigc::bind(sigc::mem_fun(*this, &Dropdown::on_selected_changed), item));

    // The pointer is passed to the set_data function with a method that
    // will destroy and disconnect the connection, so the lifetime of the
    // connection object will be managed automatically if the item or the
    // data associated with it are destroyed.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    item->set_data("connection", new sigc::connection(connection),
                   Glib::destroy_notify_delete<sigc::connection>);
    on_selected_changed(item);
}

void Dropdown::on_unbind_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto* connection =
        static_cast<sigc::connection*>(item->get_data("connection"));
    if (connection != nullptr) {
        connection->disconnect();
        item->set_data("connection", nullptr);
    }
}

void Dropdown::on_selected_changed(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto* hbox = dynamic_cast<Gtk::Box*>(item->get_child());
    if (hbox == nullptr) {
        return;
    }

    auto* checkmark = dynamic_cast<Gtk::Image*>(hbox->get_last_child());
    if (checkmark == nullptr) {
        return;
    }
    checkmark->set_visible(get_selected_item() == item->get_item());
}
// NOLINTEND(readability-convert-member-functions-to-static)
