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

#include <string>
#include <ostream>

namespace Logger {
    enum LogLevel {DEBUG, INFO, WARN, ERR, NONE};
    class LogStream {
        LogStream(LogLevel lvl, std::ostream& out) : lvl{lvl}, out{out} {}
        template<typename T>
        LogStream& operator<<(const T& rhs) {
            if(logLevel <= lvl) {
                out << rhs;
            }
            return *this;
        }
        LogLevel lvl;
        std::ostream& out;
        static LogLevel logLevel;
        friend void setLogLevel(LogLevel lvl);
        friend class Logger;
    };

    class Logger {
        public:
            Logger(LogLevel lvl, std::ostream& out) : lvl{lvl}, stream{lvl, out} {}
            void operator()(const std::string& msg);
            template<typename T>
            LogStream& operator<<(const T& rhs) {
                stream << log_header.at(lvl) << rhs;
                return stream;
            }
        private:
            LogLevel lvl;
            LogStream stream;
            static const std::unordered_map<LogLevel, std::string> log_header;
    };

    class NoLog {
        public:
            template<typename T>
            NoLog& operator<<(const T&) {
                return *this;
            }
            void operator()(const std::string&) {}
    };

    void setLogLevel(LogLevel lvl);

    extern Logger debug;
    extern Logger info;
    extern Logger warning;
    extern Logger error;
};
