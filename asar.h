#ifndef ASAR_H_INCLUDED
#define ASAR_H_INCLUDED

#include <rapidjson/document.h>
#include <dirent.h>
#include <string>
#include <fstream>
#include <vector>


class asarArchive {

private:
	std::ifstream m_ifsInputFile;
	size_t m_headerSize = 0;
	size_t m_szOffset = 0;
	bool m_extract = true;
	void unpackFiles( rapidjson::Value& object, const std::string &sPath );
	bool createJsonHeader( const std::string &sPath, std::string &sHeader, std::vector<std::string> &vFileList );
	size_t numSubfile( DIR* dir );

public:
	bool unpack( const std::string &sArchivePath, std::string sExtractPath = "" );
	bool pack( const std::string &sFinalName, const std::string &sPath );
	bool list( const std::string &sArchivePath );

};

#endif // ASAR_H_INCLUDED
