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

#ifndef ASAR_H_INCLUDED
#define ASAR_H_INCLUDED

#include <rapidjson/document.h>
#include <string>
#include <fstream>
#include <vector>


class asarArchive {

private:
	typedef struct {
		std::string path;
		size_t size;
		size_t offset;
		char type;  // 'F' regular file
					// 'L' symbolic link
					// 'X' executable file
					// 'D' directory (empty)
		std::string link_target;
	} fileEntry_t;

	std::ifstream m_ifsInputFile;
	size_t m_headerSize = 0;

	int getFiles( rapidjson::Value& object, std::vector<fileEntry_t> &vFileList, const std::string &sPath );
	bool unpackFiles( std::vector<fileEntry_t> &vFileList );
	bool unpackSingleFile( const fileEntry_t &file, const std::string &sOutPath );

	bool createJsonHeader(
		const std::string &sPath,
		std::string &sHeader,
		size_t &szOffset,
		std::vector<fileEntry_t> &vFileList,
		const char *unpack,
		const char *unpackDir,
		bool excludeHidden);

public:
	bool unpack( const std::string &sArchivePath, std::string sOutPath, std::string sExtractFile = "" );
	bool pack( const std::string &sPath, const std::string &sArchivePath, const char *unpack, const char *unpackDir, bool excludeHidden);
	bool list( const std::string &sArchivePath );

};

#endif // ASAR_H_INCLUDED
