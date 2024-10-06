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

Dropdown::Dropdown(BaseObjectType* cobj,
                   const Glib::RefPtr<Gtk::Builder>& /*builder*/)
    : Gtk::DropDown(cobj) {
    m_list_store = Gio::ListStore<ListItemModel>::create();
    set_model(m_list_store);

    // Factory for displaying selected item:
    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::mem_fun(*this, &DropDown::on_setup_selected));
    factory->signal_bind().connect(
        sigc::mem_fun(*this, &DropDown::on_bind_selected));
    set_factory(factory);

    // Factory for displaying list of items:
    factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(
        sigc::mem_fun(*this, &DropDown::on_setup_list_item));
    factory->signal_bind().connect(
        sigc::mem_fun(*this, &DropDown::on_bind_list_item));
    factory->signal_unbind().connect(
        sigc::mem_fun(*this, &DropDown::on_unbind_list_item));
    set_list_factory(factory);
}

void Dropdown::append(const std::string& option,
                      const std::string& description) {
    m_list_store->append(ListItemModel::create(option, description));
}

void Dropdown::remove_all() { m_list_store->clear(); }

std::optional<std::pair<std::string, std::string>>
Dropdown::get_active_values() {
    const auto selected = get_selected();
    if (GTK_INVALID_LIST_POSITION) {
        return {};
    }
    const auto selected_item = m_list_store->get_item(selected);
    return {selected_item->m_option, selected_item->m_description};
}

void Dropdown::on_setup_selected(const Glib::RefPtr<Gtk::ListItem>& item) {
    item->set_child(*Gtk::make_managed<Gtk::Label>());
}

void Dropdown::on_bind_selected(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto selection = std::dynamic_pointer_cast<ListItemModel>(item->get_item());
    if (!selection) {
        return;
    }
    auto label = dynamic_cast<Gtk::Label*>(item->get_child());
    if (!label) {
        return;
    }
    label->set_text(selection->m_option);
}

void Dropdown::on_setup_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    hbox->append(*vbox);
    auto option = Gtk::make_managed<Gtk::Label>();
    option->set_xalign(0.0);
    vbox->append(option);
    auto description = Gtk::make_managed<Gtk::Label>();
    description->set_xalign(0.0);
    description->add_css_class("dim-label");
    vbox->append(*description);
    auto checkmark = Gtk::make_managed<Gtk::Image>();
    checkmark->set_from_icon_name("object-select-symbolic");
    checkmark->set_visbible(false);
    hbox->append(*checkmark);
    item->set_child(*hbox);
}

void Dropdown::on_bind_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto selection = std::dynamic_pointer_cast<ListItemModel>(item->get_item());
    if (!selection) {
        return;
    }
    auto hbox = dynamic_cast<Gtk::Box*>(item->get_child());
    if (!hbox) {
        return;
    }
    auto vbox = dynamic_cast<Gtk::Box*>(hbox->get_first_child());
    if (!vbox) {
        return;
    }
    auto option = dynamic_cast<Gtk::Label*>(vbox->get_first_child());
    if (!option) {
        return;
    }
    option->set_text(selection->m_option);
    auto description = dynamic_cast<Gtk::Label*>(option->get_next_sibling());
    if (!description) {
        return;
    }
    description->set_text(selection->m_description);
    auto connection = property_selected_item().signal_changed().connect(
        sigc::bind(sigc::mem_fun(&Dropdown::on_selected_changed), item));
    item->set_data("connection", new sigc::connection(connection),
                   Glib::destroy_notify_delete<sigc::connection>);
    on_selected_changed(item);
}

void on_unbind_list_item(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto connection =
        static_cast<sigc::connection*>(item->get_data("connection"));
    if (connection) {
        connection->disconnect();
        list_item->set_data("connection", nullptr);
    }
}

void on_selected_changed(const Glib::RefPtr<Gtk::ListItem>& item) {
    auto hbox = dynamic_cast<Gtk::Box*>(item->get_child());
    if (!hbox) {
        return;
    }

    auto checkmark = dynamic_cast<Gtk::Image*>(hbox->get_last_child());
    if (!checkmark) {
        return;
    }
    checkmark->set_visible(get_selected_item() == item->get_item());
}
