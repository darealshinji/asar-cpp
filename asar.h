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
		char type;
		std::string link_target;
	} fileEntry_t;

	std::ifstream m_ifsInputFile;
	size_t m_headerSize = 0;
	size_t m_szOffset = 0;
	bool m_extract = true;
	bool unpackFiles( rapidjson::Value& object, const std::string &sPath, const std::string &sExtractFile = "" );
	bool createJsonHeader( const std::string &sPath, std::string &sHeader, std::vector<fileEntry_t> &vFileList );

public:
	bool unpack( const std::string &sArchivePath, std::string sExtractPath, const std::string &sFilePath = "" );
	bool pack( const std::string &sPath, const std::string &sFinalName );
	bool list( const std::string &sArchivePath );

};

#endif // ASAR_H_INCLUDED
