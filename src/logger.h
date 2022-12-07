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

#include <glibmm/ustring.h>

class Logger {
	public:
		enum LogLevel {DEBUG, WARN, ERR, NONE};
		static void setLogLevel(LogLevel);
		static void debug(const Glib::ustring& msg);
		static void warning(const Glib::ustring& msg);
		static void error(const Glib::ustring& msg);
	
	private:
		static LogLevel logLevel;
};
