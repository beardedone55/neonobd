/* This file is part of neonobd - OBD diagnostic software.
 * Copyright (C) 2022-2024  Brian LePage
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

#include "logger.hpp"
#include <string>

Logger::LogLevel Logger::LogStream::logLevel = DEBUG;

const Logger::Logger Logger::debug(DEBUG);
const Logger::Logger Logger::info(INFO);
const Logger::Logger Logger::warning(WARN);
const Logger::Logger Logger::error(ERR);

void Logger::setLogLevel(LogLevel lvl) { LogStream::logLevel = lvl; }

void Logger::Logger::operator()(const std::string& msg) const {
    *this << msg << '\n';
}
