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
	bool unpackFiles( const std::vector<fileEntry_t> &vFileList );
	bool unpackSingleFile( const fileEntry_t &file, const std::string &sOutPath );
	bool createJsonHeader( const std::string &sPath, std::string &sHeader, size_t &szOffset, std::vector<fileEntry_t> &vFileList, bool excludeHidden );

public:
	bool unpack( const std::string &sArchivePath, std::string sOutPath, std::string sExtractFile = "" );
	bool pack( const std::string &sPath, const std::string &sFinalName, bool excludeHidden );
	bool list( const std::string &sArchivePath );

};

#endif // ASAR_H_INCLUDED
