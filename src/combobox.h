#pragma once

#include <tuple>
#include <gtkmm.h>

template<class... T>
class ComboBox : public Gtk::ComboBox
{
    public:
        ComboBox()
        {
            std::apply([this](auto&&... cols){((column_record.add(cols)), ...);}, columns);

            list_store = Gtk::ListStore::create(column_record);
            set_model(list_store);
            std::apply([this](auto&&... cols){((pack_start(cols)), ...);}, columns);
        }

        ~ComboBox()
        {
        }

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



