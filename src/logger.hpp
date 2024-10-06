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

#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>

namespace Logger {
enum LogLevel { DEBUG, INFO, WARN, ERR, NONE };
class LogStream {
  public:
    template <typename T> const LogStream& operator<<(const T& rhs) const {
        if (logLevel <= lvl) {
            out << rhs;
        }
        return *this;
    }

  private:
    LogStream(LogLevel level) noexcept
        : lvl{level}, out{(level == ERR) ? std::cerr : std::clog} {}
    LogLevel lvl;
    std::ostream& out;
    static LogLevel logLevel;
    friend void setLogLevel(LogLevel lvl);
    friend class Logger;
};

class Logger {
  public:
    Logger(LogLevel level) noexcept : lvl{level}, stream{level} {}
    void operator()(const std::string& msg) const;
    template <typename T> const LogStream& operator<<(const T& rhs) const {
        stream << log_header.at(lvl) << rhs;
        return stream;
    }

  private:
    LogLevel lvl;
    LogStream stream;
    const std::unordered_map<LogLevel, std::string> log_header = {
        {DEBUG, "DEBUG: "},
        {INFO, "INFO: "},
        {WARN, "WARNING: "},
        {ERR, "ERROR: "}};
};

class NoLog {
  public:
    template <typename T> const NoLog& operator<<(const T&) const { return *this; }
    void operator()(const std::string&) const  {}
};

void setLogLevel(LogLevel lvl);

extern const Logger debug;
extern const Logger info;
extern const Logger warning;
extern const Logger error;
}; // namespace Logger
