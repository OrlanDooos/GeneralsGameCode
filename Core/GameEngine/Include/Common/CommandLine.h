/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// CommandLine.h
// The command-line interface
// Author: Matthew D. Campbell, September 2001

#pragma once

class CommandLine
{
public:

	// Uses these arguments instead of the ones of the process. Like for main(), argv[0] is the
	// program name and is skipped. Must be called before the parsing, for example when the engine
	// runs inside a library whose host process has its own command line. The strings must stay
	// valid while the engine runs.
	static void setArguments(int argc, char **argv);

	// Parses startup flags and applies the process working directory.
	static void parseCommandLineForStartup();
	static void parseCommandLineForEngineInit();

	// Returns true if command-line parsing consumed the zero-based argument index.
	// The index excludes the executable name.
	static bool wasCommandLineArgumentParsed(int argIndex);
};
