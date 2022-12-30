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
#include <vector>
#include <string>
#include <giomm.h>
#include "neonobd_types.h"

using neon::ResponseType;

class HardwareInterface {
    public:
        enum Flags { FLAGS_NONE = 0 };
        virtual bool connect(const Glib::ustring& device_name) = 0;
        sigc::connection attach_connect_complete(const sigc::slot<void(bool)>& slot) { 
            return complete_connection.connect(slot);
        }
        sigc::connection attach_user_prompt(const sigc::slot<void(const Glib::ustring&, 
                                                ResponseType, 
                                                Glib::RefPtr<void>)>& slot) { 
            return request_user_input.connect(slot);
        }
        virtual void respond_from_user(const Glib::VariantBase& response,
                                       const Glib::RefPtr<void>& signal_handle) = 0;
        virtual std::vector<char>::size_type read(std::vector<char>& buf,
                                                  std::vector<char>::size_type buf_size = 1024,
                                                  Flags flags = FLAGS_NONE) = 0; 
        virtual std::vector<char>::size_type write(const std::vector<char>& buf) = 0;
        virtual std::size_t read(std::string& buf,
                                 std::size_t buf_size = 1024,
                                 Flags flags = FLAGS_NONE) = 0; 
        virtual std::size_t write(const std::string& buf) = 0;
        virtual void set_timeout(int milliseconds) {}

    protected:
        sigc::signal<void(bool)> complete_connection;
        sigc::signal<void(const Glib::ustring&, ResponseType, Glib::RefPtr<void>)> request_user_input;

};
