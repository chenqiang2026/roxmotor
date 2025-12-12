#ifndef QFILE_H
#define QFILE_H
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>

class QFile {
public:
    QFile(const std::string& file, const int32_t& flag, const int32_t& mode);
    QFile(const std::string& folder);
    virtual ~QFile();
    bool isExisted();
    bool isFolder();
    void recurMkdir(const std::string& dir);
    int deleteDir(const char *path);
    uint64_t size();
    int32_t open();
    int32_t read(char* data, int32_t size, long offset = 0);
    int32_t write(const char* data, int32_t size, long offset = 0);
    int32_t close();
    bool isOpen() { return isOpen_;}

private:
    std::string path_;
    int32_t mode_;
    int32_t flag_;
    int32_t fd_;
    bool isOpen_;

};
#endif  // QFILE_H
