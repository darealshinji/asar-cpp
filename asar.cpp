#include <iostream>
#include <string>
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include "asar.h"

#ifdef _WIN32

#include <direct.h>
#define MKDIR(a) mkdir(a)
#define DIR_SEPARATOR '\\'

#else

#include <sys/stat.h>
#define MKDIR(a) mkdir(a,0777)
#define DIR_SEPARATOR '/'

#endif // _WIN32



// Return number of files in a folder
size_t asarArchive::numSubfile( DIR* dir ) {
	size_t uFiles = 0;
	struct dirent* file;

	long iDirPos = telldir(dir);
	rewinddir(dir);

	while ( (file = readdir(dir)) )
		uFiles++;

	seekdir(dir,iDirPos);
	return uFiles-2; // remove '.' and '..' dirs
}

bool asarArchive::createJsonHeader( const std::string &sPath, std::string &sHeader, std::vector<std::string> &vFileList ) {
	DIR* dir = opendir( sPath.c_str() );
	if ( !dir ) {
		perror("opendir()");
		return false;
	}

	struct dirent* file;
	size_t uFolderSize = numSubfile( dir );
	size_t uFileNum = 0;

	while ( (file = readdir(dir)) ) {
		if ( strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0 )
			continue;

		std::string sLocalPath = sPath;
		sLocalPath += DIR_SEPARATOR;
		sLocalPath += file->d_name;
		DIR* isDir = opendir( sLocalPath.c_str() );

		if ( isDir ) {
			sHeader += '"';
			sHeader += file->d_name;
			sHeader += "\":{\"files\":{";
			closedir( isDir );
			createJsonHeader( sLocalPath, sHeader, vFileList );
			sHeader += "}}";
		} else {
			std::ifstream ifsFile( sLocalPath, std::ios::binary | std::ios::ate );
			if ( !ifsFile.is_open() ) {
				std::cerr << "cannot open file for reading: " << sLocalPath << std::endl;
				closedir(dir);
				return false;
			}

			size_t szFile = ifsFile.tellg();
			ifsFile.close();

			sHeader += '"';
			sHeader += file->d_name;
			sHeader += "\":{\"size\":" + std::to_string(szFile) + ",\"offset\":\"" + std::to_string(m_szOffset) + "\"}";
			m_szOffset += szFile;
			vFileList.push_back( sLocalPath );
		}

		if ( ++uFileNum < uFolderSize )
			sHeader.push_back(',');
	}

	closedir(dir);

	return true;
}

void asarArchive::unpackFiles( rapidjson::Value& object, const std::string &sPath ) {
	if ( !object.IsObject() ) // how ?
		return;

	if ( m_extract && !sPath.empty() )
		MKDIR( sPath.c_str() );

	for ( auto itr = object.MemberBegin(); itr != object.MemberEnd(); ++itr ) {
		std::string sFilePath = sPath + itr->name.GetString();
		rapidjson::Value& vMember = itr->value;
		if ( vMember.IsObject() ) {
			if ( vMember.HasMember("files") ) {
				if ( m_extract )
					MKDIR( sFilePath.c_str() );

				unpackFiles( vMember["files"], sFilePath + DIR_SEPARATOR );
			} else {
				if ( !( vMember.HasMember("size") && vMember.HasMember("offset") && vMember["size"].IsInt() && vMember["offset"].IsString() ) )
					continue;

				if ( !m_extract ) {
					std::cout << '\t' << sFilePath << std::endl;
					continue;
				}

				size_t uSize = vMember["size"].GetUint();
				int uOffset = std::stoi( vMember["offset"].GetString() );

				char fileBuf[uSize];
				m_ifsInputFile.seekg(m_headerSize + uOffset);
				m_ifsInputFile.read(fileBuf, uSize);
				std::ofstream ofsOutputFile( sFilePath, std::ios::trunc | std::ios::binary );

				if ( !ofsOutputFile ) {
					std::cerr << "Error when writing to file " << sFilePath << std::endl;
					continue;
				}
				ofsOutputFile.write( fileBuf, uSize );
				ofsOutputFile.close();
			}
		}
	}
}

// Unpack archive to a specific location
bool asarArchive::unpack( const std::string &sArchivePath, std::string sExtractPath ) {
	m_ifsInputFile.open( sArchivePath, std::ios::binary );
	if ( !m_ifsInputFile ) {
		std::cerr << "cannot open file: " << sArchivePath << std::endl;
		return false;
	}

	char sizeBuf[8];
	m_ifsInputFile.read( sizeBuf, 8 );
	uint32_t uSize = *(uint32_t*)(sizeBuf + 4) - 8;

	m_headerSize = uSize + 16;
	char headerBuf[uSize + 1];
	m_ifsInputFile.seekg(16); // skip header
	m_ifsInputFile.read(headerBuf, uSize);
	headerBuf[uSize] = 0; // append nul byte to end

	rapidjson::Document json;
	rapidjson::ParseResult res = json.Parse( headerBuf );
	if ( !res ) {
		std::cout << rapidjson::GetParseError_En(res.Code()) << std::endl;
		return false;
	}

	if ( !sExtractPath.empty() && sExtractPath.back() != DIR_SEPARATOR )
		sExtractPath.push_back(DIR_SEPARATOR);

	unpackFiles( json["files"], sExtractPath );
	m_szOffset = 0;

	m_ifsInputFile.close();

	return true;
}

// Pack archive
bool asarArchive::pack( const std::string &sFinalName, const std::string &sPath ) {
	std::vector<std::string> vFileList;
	std::string sHeader = "{\"files\":{";

	if ( !createJsonHeader( sPath, sHeader, vFileList ) )
		return false;

	sHeader += "}}";

	std::ofstream ofsOutputFile( sFinalName, std::ios::binary | std::ios::trunc );
	if ( !ofsOutputFile.is_open() ) {
		std::cerr << "cannot open file for writing: " << sFinalName << std::endl;
		return false;
	}

	char cHeader[16];
	char *p = cHeader;

	// offset 0x00
	uint32_t uSize = 4;
	memcpy( p, &uSize, 4 );
	p = cHeader + 4;

	// offset 0x04
	uSize = sHeader.size() + 8;
	memcpy( p, &uSize, 4 );
	p += 4;

	// offset 0x08
	uSize -= 4;
	memcpy( p, &uSize, 4 );
	p += 4;

	// offset 0x0C
	uSize -= 4;
	memcpy( p, &uSize, 4 );

	ofsOutputFile.write( cHeader, 16 );
	ofsOutputFile << sHeader;

	for (const auto &e : vFileList) {
		std::ifstream ifsFile( e, std::ios::binary | std::ios::ate );

		if ( !ifsFile.is_open() ) {
			std::cerr << "cannot open file for reading: " << e << std::endl;
			ofsOutputFile.close();
			return false;
		}

		size_t szFile = ifsFile.tellg();
		ifsFile.seekg(0, std::ios::beg);

		for (size_t i=0; i < szFile; ++i)
			ofsOutputFile.put(ifsFile.get());

		ifsFile.close();
	}

	ofsOutputFile.close();

	return true;
}

// List archive content
bool asarArchive::list( const std::string &sArchivePath ) {
	m_extract = false;
	std::cout << sArchivePath << ':' << std::endl;
	bool ret = unpack( sArchivePath );
	m_extract = true;

	return ret;
}
