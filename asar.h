#ifndef ASAR_H_INCLUDED
#define ASAR_H_INCLUDED

#include <rapidjson/document.h>
#include <string>
#include <fstream>
#include <utility>
#include <vector>


class asarArchive {

private:
	std::ifstream m_ifsInputFile;
	size_t m_headerSize = 0;
	size_t m_szOffset = 0;
	bool m_extract = true;
	bool unpackFiles( rapidjson::Value& object, const std::string &sPath, const std::string &sExtractFile = "" );
	bool createJsonHeader( const std::string &sPath, std::string &sHeader,
		std::vector< std::pair<std::string, size_t> > &vFileList );

public:
	bool unpack( const std::string &sArchivePath, std::string sExtractPath, const std::string &sFilePath = "" );
	bool pack( const std::string &sPath, const std::string &sFinalName );
	bool list( const std::string &sArchivePath );

};

#endif // ASAR_H_INCLUDED
