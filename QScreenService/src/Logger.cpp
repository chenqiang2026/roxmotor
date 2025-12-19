#include "../inc/Logger.h"
#include <cstdint>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <cstring> 

slog2_buffer_t slog_handle;
slog2_buffer_set_config_t slog_config;

Logger& Logger::getInstance() 
{
    static Logger instance;
    return instance;
}

void Logger::setMaxSize(uint32_t size) 
{
    size_ = size;
    fp_ = -1;
    fileId_ = 0;
    fileName_ = "";
    setSlog();
}

void Logger::setSlog() 
{
    memset(&slog_config, 0, sizeof(slog_handle));
    slog_config.num_buffers = 1;
    slog_config.verbosity_level = LOG_DEBUG;
    // The name will be used by sLoggerger2 (a resource manager ) to create a shared device in path: /dev/shmem/sLoggerger2/test_ResMgr.${pid}
    slog_config.buffer_set_name = "ScreenService";
    slog_config.buffer_config[0].buffer_name = "ScreenService";
    slog_config.buffer_config[0].num_pages = 1024;
    slog2_register(&slog_config, &slog_handle, 0);
    slog2_set_verbosity(slog_handle, LOG_DEBUG);
    slog2_set_default_buffer(slog_handle);
}

void Logger::setLogOutputStandard(uint32_t standard) 
{
    mLogOutputStandard_ = standard;
}

bool Logger::recurMkdir(const std::string& dir) 
{
    unsigned int pos = dir.rfind('/');
    std::string upperDir = dir.substr(0, pos);
    if (upperDir == "") { // upper folder is root dir
        return true; 
    }

    int ret = true;
    if (0 != access(upperDir.c_str(), F_OK)) {
        ret = recurMkdir(upperDir);
    }

    if ((pos + 1) == dir.size()) {
        //no sub folder do not create, return
        return ret; 
    }
    if (ret == true) {
        // upper folder is existed, create this dir
        ret = (0 == ::mkdir(dir.c_str(), 0777)) ? true : false;
    }
    return ret;
    //::mkdir(dir.c_str(), 0777);
}

bool Logger::open() 
{
    if (0 != access(LOG_FOLDER_PARTITION.c_str(), F_OK)) {
        return false;
    }
    if (0 != access(LOG_FOLDER_PATH.c_str(), F_OK)) {
        if (false == recurMkdir(LOG_FOLDER_PATH)) {
            return false;
        }
    }

    if (0 < (fp_ = ::open(fileName_.c_str(), O_CREAT | O_WRONLY, S_IREAD|S_IWRITE))) {
        return true;
    }
    return false;
}

bool Logger::close() 
{
    if (-1 != access(fileName_.c_str(), F_OK)) {
        if (0 == ::close(fp_)) {
            return true;
        }
    }
    return false;
}

bool Logger::fileFullHandling() 
{
    uint32_t id = 0;
    std::string deleteName = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(id) + LOG_SUFFIX;
    if (0 != remove(deleteName.c_str())) {
        return false;
    }

    for(auto index = 0; index < LOG_FILE_MAX_NUMBER - 1; index++) {
        std::string oldName = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(index + 1) + LOG_SUFFIX;
        std::string newName = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(index) + LOG_SUFFIX;
        if (0 != rename(oldName.c_str(), newName.c_str())) {
            return false;
        }
    }
    fileName_ = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(LOG_FILE_MAX_NUMBER - 1) + LOG_SUFFIX;
    fileId_ = LOG_FILE_MAX_NUMBER - 1;
    return true;
}

bool Logger::updateCurrentFile() 
{
    if (std::string("") == fileName_) {
        // check file number
        for (auto index = 0; index < LOG_FILE_MAX_NUMBER; index++) {
            std::string fileNameIndex = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(index) + LOG_SUFFIX;
            fileId_ = index;
            if (-1 == access(fileNameIndex.c_str(), F_OK)) {
                // get new file
                fileName_ = fileNameIndex;
                bool ret = open();
                return ret;
            }
            // all file is existed
            if (index == LOG_FILE_MAX_NUMBER - 1) {
                bool ret = fileFullHandling();
                if (ret == false) {
                    return false;
                }
                ret = open();
                if (ret == false) {
                    return false;
                }
            }
        }
    }

    struct stat statData;
    stat(fileName_.c_str(), &statData);
    // check file size, if it exceeds MAX SIZE, it should write into next file
    if (statData.st_size >= size_) {
        // close current file
        close();
        if (fileId_ == LOG_FILE_MAX_NUMBER - 1) {
            bool ret = fileFullHandling();
            if (ret == false) {
                return false;
            }
        } else {
            fileId_ = (fileId_ + 1)%LOG_FILE_MAX_NUMBER;
            fileName_ = LOG_FOLDER_PATH + LOG_PREFIX + std::to_string(fileId_) + LOG_SUFFIX;
        }
        // open new file
        bool ret = open();
        return ret;
    }

    return true;
}

bool Logger::checkLogOutput(const int level)
{
    return (level >= mLogOutputStandard_) ? true : false;
}

void Logger::write(const int level, const char* data, const int length) 
{
    bool ret =  updateCurrentFile();
    if (ret == true) {
        int wrote = -1;
        if(length != (wrote = ::write(fp_, reinterpret_cast<void *>(const_cast<char *>(data)), length))) {
            // error handling
            printf("fwrite %d bytes, existing error, (%d):%s\n", wrote, errno, strerror(errno));
        }
    }
    printf("%s", data);
}

