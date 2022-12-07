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
#include "logger.h"

Logger::LogLevel Logger::logLevel = Logger::DEBUG;

void Logger::setLogLevel(Logger::LogLevel lvl) {
	logLevel = lvl;
}

void Logger::debug(const Glib::ustring& msg) {
	if(logLevel <= DEBUG) {
		std::clog << "DEBUG: " << msg << '\n';
	}
}

void Logger::warning(const Glib::ustring& msg) {
	if(logLevel <= WARN) {
		std::clog << "WARNING: " << msg << '\n';
	}
}

void Logger::error(const Glib::ustring& msg) {
	if(logLevel <= ERR) {
		std::cerr << "ERROR: " << msg << '\n';
	}
}
