#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <utility>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include "asar.h"

#ifdef _WIN32
# include <direct.h>
// assuming Little Endian for Windows
# define htole32(x) x
# define le32toh(x) x
# define DIR_SEPARATOR '\\'
#else
# include <endian.h>
# include <sys/stat.h>
# define _mkdir(a) mkdir(a,0777)
# define DIR_SEPARATOR '/'
#endif // _WIN32

#define BUFF_SIZE (512*1024)


bool asarArchive::createJsonHeader( const std::string &sPath, std::string &sHeader, std::vector< std::pair<std::string, size_t> > &vFileList ) {
	DIR* dir = opendir( sPath.c_str() );
	if ( !dir ) {
		perror(sPath.c_str());
		return false;
	}

	struct dirent* file;
	size_t uFileNum = 0;
	std::vector<std::string> entries;

	while ( (file = readdir(dir)) ) {
		if ( strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0 )
			entries.push_back(file->d_name);
	}

	std::sort(entries.begin(), entries.end());
	const size_t uFolderSize = entries.size();

	for ( const auto &e : entries ) {
		std::string sLocalPath = sPath;
		sLocalPath += DIR_SEPARATOR;
		sLocalPath += e;
		DIR* isDir = opendir( sLocalPath.c_str() );

		if ( isDir ) {
			sHeader += '"';
			sHeader += e;
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
			sHeader += e;
			sHeader += "\":{\"size\":" + std::to_string(szFile) + ",\"offset\":\"" + std::to_string(m_szOffset) + "\"}";
			m_szOffset += szFile;

			vFileList.push_back( {sLocalPath, szFile} );
		}

		if ( ++uFileNum < uFolderSize )
			sHeader.push_back(',');
	}

	closedir(dir);

	return true;
}

bool asarArchive::unpackFiles( rapidjson::Value& object, const std::string &sPath, const std::string &sExtractFile ) {
	if ( !object.IsObject() ) // how ?
		return false;

	if ( m_extract && !sPath.empty() )
		_mkdir( sPath.c_str() );

	for ( auto itr = object.MemberBegin(); itr != object.MemberEnd(); ++itr ) {
		rapidjson::Value& vMember = itr->value;
		if ( !vMember.IsObject() ) continue;

		std::string sFilePath = sPath + itr->name.GetString();
		const char *pPath = sFilePath.c_str();
		if ( !sExtractFile.empty() ) pPath += sPath.size();

		if ( vMember.HasMember("files") ) {
			if ( m_extract && sExtractFile.empty() )
				_mkdir( sFilePath.c_str() );

			unpackFiles( vMember["files"], sFilePath + DIR_SEPARATOR, sExtractFile );
		} else {
			if ( !( vMember.HasMember("size") && vMember.HasMember("offset") && vMember["size"].IsInt() && vMember["offset"].IsString() ) )
				continue;

			if ( !sExtractFile.empty() && sExtractFile != sFilePath )
				continue; // not the file we want to extract -> continue

			if ( sExtractFile.empty() && !m_extract ) {
				std::cout << sFilePath << std::endl;
				continue;
			}

			size_t uSize = vMember["size"].GetUint();
			int uOffset = std::stoi( vMember["offset"].GetString() );
			std::ofstream ofsOutputFile( pPath, std::ios::trunc | std::ios::binary );

			if ( !ofsOutputFile ) {
				std::cerr << "Error when writing to file " << sFilePath << std::endl;
				return false;
			}

			if (uSize > 0) {
				char fileBuf[BUFF_SIZE];
				m_ifsInputFile.seekg(m_headerSize + uOffset);

				while (uSize > BUFF_SIZE) {
					m_ifsInputFile.read(fileBuf, BUFF_SIZE);
					ofsOutputFile.write(fileBuf, BUFF_SIZE);
					uSize -= BUFF_SIZE;
				}

				m_ifsInputFile.read(fileBuf, uSize);
				ofsOutputFile.write(fileBuf, uSize);
			}

			ofsOutputFile.close();
			if (!sExtractFile.empty()) return true;
		}
	}

	return true;
}

// Unpack archive to a specific location
bool asarArchive::unpack( const std::string &sArchivePath, std::string sExtractPath, const std::string &sFilePath ) {
	m_ifsInputFile.open( sArchivePath, std::ios::binary );
	if ( !m_ifsInputFile ) {
		std::cerr << "cannot open file: " << sArchivePath << std::endl;
		return false;
	}

	// first 16 bytes consist of 4 numbers stored as uint32_t little endian:
	// uHdr1 = 4
	// uHdr2 = <JSON header size> + 8
	// uHdr3 = <JSON header size> + 4
	// uSize = <JSON header size>
	char sizeBuf[16];

	if ( !m_ifsInputFile.read( sizeBuf, 16 ) ) {
		std::cerr << "unexpected file header size" << std::endl;
		m_ifsInputFile.close();
		return false;
	}

	const uint32_t uHdr1 = le32toh( *(reinterpret_cast<uint32_t*>(sizeBuf)) );
	const uint32_t uHdr2 = le32toh( *(reinterpret_cast<uint32_t*>(sizeBuf + 4)) );
	const uint32_t uHdr3 = le32toh( *(reinterpret_cast<uint32_t*>(sizeBuf + 8)) );
	const uint32_t uSize = le32toh( *(reinterpret_cast<uint32_t*>(sizeBuf + 12)) );

	if ( uHdr1 != 4 || uHdr2 != (uSize + 8) || uHdr3 != (uSize + 4) ) {
		std::cerr << "unexpected file header data" << std::endl;
		m_ifsInputFile.close();
		return false;
	}

	char headerBuf[uSize + 1] = {0}; // initialize with zeros
	m_headerSize = uSize + 16;

	if (!m_ifsInputFile.read(headerBuf, uSize)) {
		std::cerr << "JSON header data too short" << std::endl;
		m_ifsInputFile.close();
		return false;
	}

	rapidjson::Document json;
	rapidjson::ParseResult res = json.Parse( headerBuf );
	if ( !res ) {
		std::cout << rapidjson::GetParseError_En(res.Code()) << std::endl;
		m_ifsInputFile.close();
		return false;
	}

	if ( !sExtractPath.empty() && sExtractPath.back() != DIR_SEPARATOR )
		sExtractPath.push_back(DIR_SEPARATOR);

	if ( !sFilePath.empty() )
		m_extract = false;

	bool ret = unpackFiles( json["files"], sExtractPath, sFilePath );
	m_szOffset = 0;
	m_extract = true;

	m_ifsInputFile.close();

	return ret;
}

// Pack archive
bool asarArchive::pack( const std::string &sPath, const std::string &sFinalName ) {
	std::vector< std::pair<std::string, size_t> > vFileList;
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
	uint32_t uSize = htole32(4);
	memcpy( p, &uSize, 4 );
	p = cHeader + 4;

	// offset 0x04
	uSize = htole32( sHeader.size() + 8 );
	memcpy( p, &uSize, 4 );
	p += 4;

	// offset 0x08
	uSize = htole32( sHeader.size() + 4 );
	memcpy( p, &uSize, 4 );
	p += 4;

	// offset 0x0C
	uSize = htole32( sHeader.size() );
	memcpy( p, &uSize, 4 );

	ofsOutputFile.write( cHeader, 16 );
	ofsOutputFile << sHeader;

	char fileBuf[BUFF_SIZE];

	for (const auto &e : vFileList) {
		std::ifstream ifsFile( e.first, std::ios::binary | std::ios::ate );

		if ( !ifsFile.is_open() ) {
			std::cerr << "cannot open file for reading: " << e.first << std::endl;
			ofsOutputFile.close();
			return false;
		}

		size_t szFile = ifsFile.tellg();

		if (szFile != e.second) {
			std::cerr << "file size does not match: " << e.first << ": " << e.second
				<< " -> " << szFile << " was expected" << std::endl;
			ifsFile.close();
			ofsOutputFile.close();
			return false;
		}

		if (szFile > 0) {
			ifsFile.seekg(0, std::ios::beg);

			while (szFile > BUFF_SIZE) {
				ifsFile.read(fileBuf, BUFF_SIZE);
				ofsOutputFile.write(fileBuf, BUFF_SIZE);
				szFile -= BUFF_SIZE;
			}

			ifsFile.read(fileBuf, szFile);
			ofsOutputFile.write(fileBuf, szFile);
		}

		ifsFile.close();
	}

	ofsOutputFile.close();

	return true;
}

// List archive content
bool asarArchive::list( const std::string &sArchivePath ) {
	m_extract = false;
	bool ret = unpack( sArchivePath, "" );
	m_extract = true;
	return ret;
}
