/* Copyright 2022 - Brian LePage */

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
