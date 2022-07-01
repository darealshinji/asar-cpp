#include <iostream>
#include <string>
#include "asar.h"


static int printHelp(const char *argv0) {
	std::cout << "Usage: " << argv0 << " [command] ...\n\n"
		"Commands:\n"
		"  pack|p <dir> <output>                 create asar archive\n"
		"  list|l <archive>                      list files of asar archive\n"
		"  extract|e <archive> <dest>            extract archive\n"
		"  extract-file|ef <archive> <filename>  extract one file from archive"
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

	if ( strcmp(argv[1], "p") == 0 || strcmp(argv[1], "pack") == 0 ) {
		if (argc != 4)
			return printHelp(argv[0]);

		std::string out = argv[3];
		if ( out.size() < 5 || strcmp(argv[3] + out.size()-5, ".asar") != 0 )
			out += ".asar";

		if ( !archive.pack( argv[2], out ) )
			return 1;
	}

	else if ( strcmp(argv[1], "l") == 0 || strcmp(argv[1], "list") == 0 ) {
		if (argc != 3)
			return printHelp(argv[0]);
		if ( !archive.list( argv[2] ) )
			return 1;
	}

	else if ( strcmp(argv[1], "e") == 0 || strcmp(argv[1], "extract") == 0 ) {
		if (argc != 4)
			return printHelp(argv[0]);
		if ( !archive.unpack( argv[2], argv[3] ) )
			return 1;
	}

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

