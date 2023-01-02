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

#include <iostream>
#include <unordered_map>
#include "logger.h"

Logger::LogLevel Logger::LogStream::logLevel = DEBUG;

Logger::Logger Logger::debug(DEBUG, std::clog);
Logger::Logger Logger::info(INFO, std::clog);
Logger::Logger Logger::warning(WARN, std::clog);
Logger::Logger Logger::error(ERR, std::cerr);

void Logger::setLogLevel(LogLevel lvl) {
    LogStream::logLevel = lvl;
}

const std::unordered_map<Logger::LogLevel, std::string> Logger::Logger::log_header =
    {{DEBUG, "DEBUG: "},
     {INFO, "INFO: "},
     {WARN, "WARNING: "},
     {ERR, "ERROR: "}};

void Logger::Logger::operator()(const std::string& msg) {
    *this << msg << '\n';
}

