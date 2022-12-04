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

class HardwareInterface {
    public:
        enum Flags { FLAGS_NONE = 0 };
        enum ResponseType { 
            USER_YN,
            USER_STRING,
            USER_INT,
            USER_NONE
        };
        virtual ~HardwareInterface() { };
        virtual bool connect(const sigc::slot<void(bool)> & connect_complete,
                             const sigc::slot<void(Glib::ustring, ResponseType, const void*)> & user_prompt) = 0;
        virtual void respond_from_user(const Glib::VariantBase & response,
                                       const Glib::RefPtr<Glib::Object>  & signal_handle) = 0;
        virtual std::vector<char>::size_type read(std::vector<char> & buf,
                                                  std::vector<char>::size_type buf_size = 1024,
                                                  Flags flags = FLAGS_NONE) = 0; 
        virtual std::vector<char>::size_type write(const std::vector<char> & buf) = 0;
        virtual std::size_t read(std::string & buf,
                                 std::size_t buf_size = 1024,
                                 Flags flags = FLAGS_NONE) = 0; 
        virtual std::size_t write(const std::string & buf) = 0;
};
