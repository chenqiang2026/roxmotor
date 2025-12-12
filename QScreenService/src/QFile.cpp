#include "../inc/QFile.h"
#include <cstdint>
#include <string>
#include <assert.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstring>
#include <iostream>

QFile::QFile(const std::string& file, const int32_t& flag, const int32_t& mode) :
    path_(file),
    mode_(mode),
    flag_(flag),
    fd_(-1),
    isOpen_(false) {

}
QFile::QFile(const std::string& folder) :
     path_(folder),
    isOpen_(false) {
}

QFile::~QFile() {

}

bool QFile::isExisted() {
    bool existed = true;
    struct stat info;
    if (stat(path_.c_str(), &info) != 0) {
        existed = false;
    }
    return existed;
}

bool QFile::isFolder() {
    bool existed = false;
    struct stat info;
    if (stat(path_.c_str(), &info) != 0) {
    } else if (S_ISDIR(info.st_mode)) {
        existed = true;
    } else if (S_ISREG(info.st_mode)) {
        existed = true;
    }
    return existed;
}

void QFile::recurMkdir(const std::string& dir) {
    int pos = dir.rfind('/');
    std::string upperDir = dir.substr(0, pos);
    QFile _upperDir(upperDir);
    // upper folder is existed, create it
    if (_upperDir.isExisted()) {
        ::mkdir(dir.c_str(), 0777);
        std::cout << "isExisted create folder  " << dir << std::endl;
    } else {
        recurMkdir(upperDir);
        ::mkdir(dir.c_str(), 0777);
        std::cout << "NoExisted create folder  " << dir << std::endl;
    }
}

int QFile::deleteDir(const char *path) {
    DIR *d = opendir(path);
    if(d == NULL) {
        return -1;
    }
    struct dirent *dt = NULL;
    while (nullptr != (dt = readdir(d))) {
        struct stat st;
        char filename[1024];
        snprintf(filename, 1024, "%s/%s", path, dt->d_name);
        stat(filename, &st);
        if(S_ISREG(st.st_mode)) {
            int ret = remove(filename);
            if (ret != 0) {
            }
        } else if(S_ISDIR(st.st_mode)) {
            if(strcmp(dt->d_name,".") == 0 || strcmp(dt->d_name, "..") == 0)
                continue;
            deleteDir(filename);
        }
    }
    closedir(d);
    int ret = remove(path);
    if (ret != 0) {
    }
    return 0;
}

uint64_t QFile::size() {
    struct stat info;
    if (stat(path_.c_str(), &info) != 0) {
    } else if (S_ISREG(info.st_mode)) {
        return info.st_size;
    }
    return 0;
}

int32_t QFile::open() {
    int32_t code = -1;
    if (!isOpen_ && !path_.empty()) {
        fd_ = ::open(path_.c_str(), flag_, mode_);
        if (fd_ < 0) {
        } else {
            isOpen_ = true;
            code = 1;
        }
    }
    return code;
}

int32_t QFile::read(char* data, int32_t size, long offset) {
     int32_t readSize = 0;  
    if (isOpen_) {
        readSize = ::read(fd_, reinterpret_cast<void *>(data), size);
        if (0 > readSize) {
        } else if (0 == readSize) {
        } else if (readSize != size) {
        }
    }
    return readSize;
}

int32_t QFile::write(const char* data, int32_t size, long offset) {
    int32_t code = -1;
    if (isOpen_) {
        int32_t wroteSize = ::write(fd_, reinterpret_cast<const void *>(data), size);
        if (wroteSize != size) {
        } else {
            code = 0;
        }
    }
    return code;
}

int32_t QFile::close() {
    int32_t code = -1;
    if (isOpen_) {
        code = ::close(fd_);
        if (0 != code) {
        } else {
            code = 0;
        }
    }
    isOpen_ = false;
    return code;
}
