/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022  Brian LePage
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

#include <tuple>
#include <optional>
#include <gtkmm/combobox.h>
#include <glibmm/refptr.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodelcolumn.h>
#include <gtkmm/builder.h>

template<class... T>
class ComboBox : public Gtk::ComboBox
{
    public:
        ComboBox(BaseObjectType* cobj, const Glib::RefPtr<Gtk::Builder>& ui) :
            Gtk::ComboBox(cobj)
        {
            std::apply([this](auto&&... cols){((column_record.add(cols)), ...);}, columns);

            list_store = Gtk::ListStore::create(column_record);
            set_model(list_store);
            std::apply([this](auto&&... cols){((pack_start(cols)), ...);}, columns);
        }
        ComboBox(const ComboBox&) = delete;
        ComboBox& operator=(const ComboBox&) = delete;
        ~ComboBox() = default;

        void append(const T&... args)
        {
            auto row = *list_store->append();
            assignValues(row, columns, std::index_sequence_for<T...>{}, args...);
        }

        void remove_all()
        {
            list_store->clear();
        }

        std::optional<std::tuple<T...>> get_active_values()
        {
            auto row = get_active();
            return getValues(row, columns, std::index_sequence_for<T...>{});
        }
    private:
        Gtk::TreeModelColumnRecord column_record;
        std::tuple<Gtk::TreeModelColumn<T>...> columns;
        Glib::RefPtr<Gtk::ListStore> list_store;

        template<std::size_t... Is>
        void assignValues(Gtk::TreeModel::Row &row,
                          const std::tuple<Gtk::TreeModelColumn<T>...>& cols,
                          std::index_sequence<Is...>, const T&... vals)
        {
            ((row[std::get<Is>(cols)] = vals), ...);
        }
        template<std::size_t... Is>
        std::optional<std::tuple<T...>> 
        getValues(Gtk::TreeModel::iterator &it,
                  const std::tuple<Gtk::TreeModelColumn<T>...>& cols,
                  std::index_sequence<Is...>)
        {
            auto row = *it;
            if(it)
                return std::make_tuple(row[std::get<Is>(cols)]...);

            return {};
        }

};



