/**
 * MIT License
 *
 * Copyright (c) 2018 Maks-s
 * Copyright (c) 2022 djcj <djcj@gmx.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <string>
#include <regex>
#include "asar.h"

// https://en.cppreference.com/w/cpp/regex/error_type
static bool regex_check(const char *expr) {
	try {
		std::regex re(expr);
	}
	catch (const std::regex_error& e) {
		const char *errmsg = "";

#define CASE(TYPE, MSG) \
    case std::regex_constants::TYPE: errmsg = " (" #TYPE "):\n  " MSG; break

		switch (e.code()) {
			CASE(error_collate, "The expression contains an invalid collating element name");
			CASE(error_ctype, "The expression contains an invalid character class name");
			CASE(error_escape, "The expression contains an invalid escaped character or a trailing escape");
			CASE(error_backref, "The expression contains an invalid back reference");
			CASE(error_brack, "The expression contains mismatched square brackets ('[' and ']')");
			CASE(error_paren, "The expression contains mismatched parentheses ('(' and ')')");
			CASE(error_brace, "The expression contains mismatched curly braces ('{' and '}')");
			CASE(error_badbrace, "The expression contains an invalid range in a {} expression");
			CASE(error_range, "The expression contains an invalid character range (e.g. [b-a])");
			CASE(error_space, "There was not enough memory to convert the expression into a finite state machine");
			CASE(error_badrepeat, "one of *?+{ was not preceded by a valid regular expression");
			CASE(error_complexity, "The complexity of an attempted match exceeded a predefined level");
			CASE(error_stack, "There was not enough memory to perform a match");
		}

#undef CASE

		std::cerr << e.what() << errmsg << '.' << std::endl;
		return false;
	}

	return true;
}

static int printHelp(const char *argv0) {
	std::cout <<
		"Usage: " << argv0 << " [command] [options]\n"
		"\n"
		"Manipulate asar archive files\n"
		"\n"
		"Options:\n"
		"  -h, --help                            display help for command\n"
		"\n"
		"Commands:\n"
		"  pack|p [options] <dir> <output>       create asar archive\n"
		"  list|l <archive>                      list files of asar archive\n"
		"  extract-file|ef <archive> <filename>  extract one file from archive\n"
		"  extract|e <archive> <dest>            extract archive\n"
		"\n"
		"Options for command `pack':\n"
		"  --unpack=<expression>      do not pack files matching glob <expression>\n"
		"  --unpack-dir=<expression>  do not pack dirs matching glob <expression>\n"
		"  --exclude-hidden           exclude hidden files\n"
		<< std::endl;
	return 1;
}

int main( int argc, char *argv[] ) {
#ifdef _WIN32
	if ( argc == 2 ) {
		size_t iLength = strlen(argv[1]);

		if ( iLength < 5 )
			return printHelp(argv[0]);

		// if string doesn't end with .asar
		if ( strcmp(argv[1] + iLength-5, ".asar") != 0 )
			return printHelp(argv[0]);

		// unpack into the archive's directory when it's drag-&-dropped
		// or the current working directory if run from command line
		asarArchive archive;
		return archive.unpack( argv[1], "" ) ? 0 : 1;
	}
#endif

	if ( argc < 3 )
		return printHelp(argv[0]);

	asarArchive archive;

	// pack
	if ( strcmp(argv[1], "p") == 0 || strcmp(argv[1], "pack") == 0 ) {
		int shift = 0;
		bool excludeHidden = false;
		const char *unpack = nullptr;
		const char *unpackDir = nullptr;

		if ( argc < 4 )
			return printHelp(argv[0]);
		else if ( argc > 4 ) {
			for (int i = 2; i < argc - 2; i++) {
				if ( strcmp(argv[i], "--exclude-hidden") == 0 ) {
					excludeHidden = true;
					shift++;
				} else if ( strncmp(argv[i], "--unpack=", 9) == 0 && strlen(argv[i]) > 9 ) {
					unpack = argv[i] + 9;
					shift++;
				} else if ( strncmp(argv[i], "--unpack-dir=", 13) == 0 && strlen(argv[i]) > 13 ) {
					unpackDir = argv[i] + 13;
					shift++;
				} else
					return printHelp(argv[0]);
			}
		}

		// check regex for errors
		if ( (unpack && !regex_check(unpack)) || (unpackDir && !regex_check(unpackDir)) )
			return 1;

		std::string out = argv[3 + shift];

		if ( out.size() < 5 || strcmp(out.c_str() + out.size()-5, ".asar") != 0 )
			out += ".asar";

		if ( !archive.pack( argv[2 + shift], out, unpack, unpackDir, excludeHidden ) )
			return 1;
	}

	// list
	else if ( strcmp(argv[1], "l") == 0 || strcmp(argv[1], "list") == 0 ) {
		if (argc != 3)
			return printHelp(argv[0]);
		if ( !archive.list( argv[2] ) )
			return 1;
	}

	// extract all files
	else if ( strcmp(argv[1], "e") == 0 || strcmp(argv[1], "extract") == 0 ) {
		if (argc != 4)
			return printHelp(argv[0]);
		if ( !archive.unpack( argv[2], argv[3] ) )
			return 1;
	}

	// extract single file
	else if ( strcmp(argv[1], "ef") == 0 || strcmp(argv[1], "extract-file") == 0 ) {
		if (argc != 4)
			return printHelp(argv[0]);
		if ( !archive.unpack( argv[2], "", argv[3] ) )
			return 1;
	}

	else
		return printHelp(argv[0]);

	return 0;
}

