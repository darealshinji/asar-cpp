#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
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
# include <unistd.h>
# define _mkdir(a) mkdir(a,0777)
# define DIR_SEPARATOR '/'
#endif // _WIN32

#define BUFF_SIZE (512*1024)


bool asarArchive::createJsonHeader(
		const std::string &sPath,
		std::string &sHeader,
		size_t &szOffset,
		std::vector<fileEntry_t> &vFileList,
		bool excludeHidden
) {
	DIR* dir = opendir( sPath.c_str() );
	if ( !dir ) {
		perror(sPath.c_str());
		return false;
	}

	struct dirent* file;
	size_t uFileNum = 0;
	std::vector<std::string> entries;

	while ( (file = readdir(dir)) ) {
		if ( strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0 )
			continue;

#ifdef _WIN32
		if (excludeHidden) {
			std::string s = sPath + "\\";
			s += file->d_name;

			DWORD res = GetFileAttributesA(s.c_str());
			if ( res == INVALID_FILE_ATTRIBUTES || !(res & FILE_ATTRIBUTE_HIDDEN) )
				entries.push_back(file->d_name);
		} else {
			entries.push_back(file->d_name);
		}
#else
		if ( !(excludeHidden && file->d_name[0] == '.') )
			entries.push_back(file->d_name);
#endif
	}

	std::sort(entries.begin(), entries.end());
	const size_t uFolderSize = entries.size();

	for ( const auto &e : entries ) {
		std::string sLocalPath = sPath;
		sLocalPath += DIR_SEPARATOR;
		sLocalPath += e;
		DIR* isDir = opendir( sLocalPath.c_str() );

		if ( isDir ) {
			closedir( isDir );
			sHeader += "\"" + e + "\":{\"files\":{";
			createJsonHeader( sLocalPath, sHeader, szOffset, vFileList, excludeHidden );
			sHeader += "}}";
		} else {
			fileEntry_t entry;
#ifdef _WIN32
			HANDLE hFile = CreateFile(sLocalPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
			if ( hFile == INVALID_HANDLE_VALUE ) {
				std::cerr << "cannot open file for reading: " << sLocalPath << std::endl;
				closedir(dir);
				return false;
			}

			LARGE_INTEGER lFileSize;
			BOOL ret = GetFileSizeEx(hFile, &lFileSize);
			CloseHandle(hFile);

			if ( ret == FALSE ) {
				std::cerr << "cannot retrieve file size: " << sLocalPath << std::endl;
				closedir(dir);
				return false;
			}

			entry.path = sLocalPath;
			entry.size = lFileSize.QuadPart;
			sHeader += "\"" + e + "\":{\"size\":" + std::to_string(entry.size) + ",\"offset\":\"" + std::to_string(szOffset) + "\"}";
			szOffset += entry.size;
#else
			struct stat st;
			if ( lstat( sLocalPath.c_str(), &st ) == -1) {
				perror("stat()");
				closedir(dir);
				return false;
			}

			entry.path = sLocalPath;
			sHeader += "\"" + e;

			if (S_ISLNK(st.st_mode)) {
				char buf[4096];
				sHeader += "\":{\"link\":\"";

				if ( readlink( sLocalPath.c_str(), buf, sizeof(buf)-1) > 0 ) {
					entry.link_target = buf;
					sHeader += buf;
				}
				sHeader += "\"}";
				entry.size = 0;
				entry.type = 'L';
			} else {
				sHeader += "\":{\"size\":" + std::to_string(st.st_size) + ",\"offset\":\"" + std::to_string(szOffset);
				if ( st.st_mode & S_IXUSR ) {
					sHeader += "\",\"executable\":true}";
					entry.type = 'X';
				} else {
					sHeader += "\"}";
					entry.type = 'F';
				}
				szOffset += st.st_size;
				entry.size = st.st_size;
			}
#endif  // !_WIN32
			vFileList.push_back(entry);
		}

		if ( ++uFileNum < uFolderSize )
			sHeader.push_back(',');
	}

	closedir(dir);

	return true;
}

int asarArchive::getFiles( rapidjson::Value& object, std::vector<fileEntry_t> &vFileList, const std::string &sPath ) {
	if ( !object.IsObject() ) // how ?
		return -1;

	int n = 0;

	for ( auto itr = object.MemberBegin(); itr != object.MemberEnd(); ++itr, ++n ) {
		rapidjson::Value& vMember = itr->value;
		if ( !vMember.IsObject() ) continue;

		std::string sFilePath = sPath + itr->name.GetString();

		if ( vMember.HasMember("files") ) {
			int ret = getFiles( vMember["files"], vFileList, sFilePath + DIR_SEPARATOR );

			if ( ret == 0 ) {
				// create empty directory entry
				fileEntry_t file;
				file.path = sFilePath;
				file.size = 0;
				file.offset = 0;
				file.type = 'D';
				vFileList.push_back( file );
				continue;
			} else if ( ret == -1 )
				return -1;
		} else {
			fileEntry_t file;

			if ( vMember.HasMember("link") && vMember["link"].IsString() ) {
				file.path = sFilePath;
				file.size = 0;
				file.offset = 0;
				file.type = 'L';
				file.link_target = vMember["link"].GetString();
				vFileList.push_back( file );
				continue;
			} else if ( vMember.HasMember("directory") && vMember["directory"].IsString() ) {
				file.path = sFilePath;
				file.size = 0;
				file.offset = 0;
				file.type = 'D';
				vFileList.push_back( file );
				continue;
			}

			if ( !( vMember.HasMember("size") && vMember.HasMember("offset") &&
					vMember["size"].IsInt() && vMember["offset"].IsString() ) )
				continue;

			file.path = sFilePath;
			file.size = vMember["size"].GetUint();
			file.offset = std::stoi( vMember["offset"].GetString() );
			file.type = 'F';

#ifndef _WIN32
			if (vMember.HasMember("executable") && vMember["executable"].IsBool() &&
				vMember["executable"].GetBool() == true)
				file.type = 'X';
#endif

			vFileList.push_back( file );
		}
	}

	return n;
}

bool asarArchive::unpackFiles( const std::vector<fileEntry_t> &vFileList ) {
	for ( const auto &file : vFileList ) {
		//rmdir(file.path.c_str());  // in case the path exists and is a directory (must be empty)
		//unlink(file.path.c_str());  // don't accidentally write into a link target

		char *copy = strdup(file.path.c_str());
		char *p = copy;

		// like "mkdir -p"
		while (*p++) {
			if (*p == DIR_SEPARATOR) {
				*p = 0;
				_mkdir(copy);
				*p = DIR_SEPARATOR;
			}
		}
		free(copy);

		if ( !unpackSingleFile(file, file.path) )
			return false;
	}
	return true;
}

bool asarArchive::unpackSingleFile( const fileEntry_t &file, const std::string &sOutPath ) {
	if (file.type == 'L') {
#ifdef _WIN32
		// symbolic links (not .lnk files!) on Windows/NTFS are used differently
		// from Unix, so instead we create a text file with the link target
		std::ofstream ofsOutputFile( sOutPath.c_str(), std::ios::trunc );

		if ( !ofsOutputFile ) {
			std::cerr << "Error when writing to file " << sOutPath << std::endl;
			return false;
		}
		ofsOutputFile << file.link_target;
		ofsOutputFile.close();
#else
		if ( symlink( file.link_target.c_str(), sOutPath.c_str() ) != 0 ) {
			perror("symlink()");
			return false;
		}
#endif
		return true;
	} else if (file.type == 'D') {
		return (_mkdir(sOutPath.c_str()) == 0);
	}

	std::ofstream ofsOutputFile( sOutPath.c_str(), std::ios::trunc | std::ios::binary );

	if ( !ofsOutputFile ) {
		std::cerr << "Error when writing to file " << sOutPath << std::endl;
		return false;
	}

	if (file.size > 0) {
		char fileBuf[BUFF_SIZE];
		size_t uSize = file.size;
		m_ifsInputFile.seekg(m_headerSize + file.offset);

		while (uSize > BUFF_SIZE) {
			m_ifsInputFile.read(fileBuf, BUFF_SIZE);
			ofsOutputFile.write(fileBuf, BUFF_SIZE);
			uSize -= BUFF_SIZE;
		}

		m_ifsInputFile.read(fileBuf, uSize);
		ofsOutputFile.write(fileBuf, uSize);
	}

	ofsOutputFile.close();

#ifndef _WIN32
	if (file.type == 'X')
		chmod(sOutPath.c_str(), 0775);
#endif

	return true;
}

// Unpack archive to a specific location
bool asarArchive::unpack( const std::string &sArchivePath, std::string sOutPath, std::string sExtractFile ) {
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

	char *headerBuf = new char[uSize + 1](); // initialize with zeros
	m_headerSize = uSize + 16;

	if (!m_ifsInputFile.read(headerBuf, uSize)) {
		std::cerr << "JSON header data too short" << std::endl;
		m_ifsInputFile.close();
		delete headerBuf;
		return false;
	}

	rapidjson::Document json;
	rapidjson::ParseResult res = json.Parse(headerBuf);
	delete headerBuf;

	if ( !res ) {
		std::cout << rapidjson::GetParseError_En(res.Code()) << std::endl;
		m_ifsInputFile.close();
		return false;
	}

	if ( !sOutPath.empty() ) {
		if ( sOutPath.back() != DIR_SEPARATOR )
			sOutPath.push_back( DIR_SEPARATOR );

		if ( !sExtractFile.empty() )
			sExtractFile.insert(0, sOutPath);
	}

	std::vector<fileEntry_t> vFileList;
	bool ret = getFiles( json["files"], vFileList, sOutPath );

	if ( !ret ) {
		m_ifsInputFile.close();
		return false;
	}

	if ( !sExtractFile.empty() ) {
		// extract single file
		for ( const auto &e : vFileList ) {
			if ( e.path == sExtractFile ) {
				// basename
				size_t pos = sExtractFile.find_last_of(DIR_SEPARATOR);
				if ( pos != std::string::npos )
					sExtractFile.erase(0, pos+1);

				ret = unpackSingleFile( e, sExtractFile );
				break;
			}
		}
	} else if ( sOutPath.empty() ) {
		// print file list

		//auto lambda = [](const fileEntry_t &a, const fileEntry_t &b) {
		//	return strcmp(a.path.c_str(), b.path.c_str()) < 0;
		//};
		//std::sort( vFileList.begin(), vFileList.end(), lambda );
		for ( const auto &e : vFileList )
			std::cout << e.path << std::endl;
	} else {
		// extract all files
		DIR *dir = opendir( sOutPath.c_str() );

		// "Directory does not exist" is the only error we accept
		if ( !dir && errno != ENOENT ) {
			int errsv = errno;
			std::cerr << "error trying to open directory:" << std::endl;
			errno = errsv;
			perror( sOutPath.c_str() );
			m_ifsInputFile.close();
			return false;
		}

		// check if directory is empty
		if ( dir ) {
			struct dirent *ent;
			int i = 0;
			while ( (ent = readdir(dir)) ) i++;
			closedir(dir);
			if (i > 2) {
				std::cerr << "directory is not empty: " << sOutPath << std::endl;
				m_ifsInputFile.close();
				return false;
			}
		}

		ret = unpackFiles( vFileList );
	}

	m_ifsInputFile.close();

	return ret;
}

// Pack archive
bool asarArchive::pack( const std::string &sPath, const std::string &sFinalName, bool excludeHidden ) {
	std::vector<fileEntry_t> vFileList;
	std::string sHeader = "{\"files\":{";
	size_t szOffset = 0;

	if ( !createJsonHeader( sPath, sHeader, szOffset, vFileList, excludeHidden ) )
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
		if (e.type == 'L') continue;  // skip symbolic link

		std::ifstream ifsFile( e.path, std::ios::binary );

		if ( !ifsFile.is_open() ) {
			std::cerr << "cannot open file for reading: " << e.path << std::endl;
			ofsOutputFile.close();
			return false;
		}

		size_t szFile = e.size;

		if (szFile > 0) {
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
	return unpack( sArchivePath, "", "" );
}
