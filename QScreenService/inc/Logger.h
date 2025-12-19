#ifndef Logger_H
#define Logger_H

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/slog2.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

#define gettid() static_cast<int>(gettid())
//#define slog2f(a, b, c, d) 
// #define SLOG2_SHUTDOWN  0
// #define SLOG2_CRITICAL  1
// #define SLOG2_ERROR     2
// #define SLOG2_WARNING   3
// #define SLOG2_NOTICE    4
// #define SLOG2_INFO      5
// #define SLOG2_DEBUG1    6
// #define SLOG2_DEBUG2    7
// #define FILTER_CODE 77
extern slog2_buffer_t slog_handle;
#define FILTER_CODE 77

//using LogSeverity = char;
constexpr char LOG_VERBOSE        = 'V';
constexpr char LOG_INFO           = 'I';
constexpr char LOG_DEBUG          = 'D';
constexpr char LOG_WARN           = 'W';
constexpr char LOG_ERROR          = 'E';

const uint8_t LOG_LVL_V                    = 1;
const uint8_t LOG_LVL_D                    = 2;
const uint8_t LOG_LVL_I                    = 3;
const uint8_t LOG_LVL_W                    = 4;
const uint8_t LOG_LVL_E                    = 5;
const uint8_t LOG_LVL_N                    = 6;

constexpr char LOG_OUTPUTLVL      = LOG_VERBOSE;

const std::string LOG_FOLDER_PARTITION = "/screenService";
const std::string LOG_FOLDER_PATH = "/screenService/";
const std::string LOG_PREFIX = "log_screenService_";
const std::string LOG_SUFFIX = ".log";

const uint32_t LOG_FILE_MAX_NUMBER = 64;
const uint32_t SINGLE_LOG_FILE_SIZE = 16 * 1024 *  1024;
const uint32_t SINGLE_LOG_LENGTH = 512;
const uint32_t TIMESTAMP_LENGTH = 24;

#ifndef LOGGER_SIZE
#define LOGGER_SIZE(size) Logger::getInstance().setMaxSize(size)
#endif
#ifndef SCREEN_SLOG_INIT
#define SCREEN_SLOG_INIT() Logger::getInstance().setSlog()
#endif

#ifndef SCREEN_LOGV     
#define SCREEN_LOGV(fmt, ...) do{ \
        if (!Logger::getInstance().checkLogOutput(LOG_LVL_V)) { \
                break; \
        } \
        char szLog[SINGLE_LOG_LENGTH] = {0}; \
        char szCoreLog[SINGLE_LOG_LENGTH] = {0}; \
        char timestamp[TIMESTAMP_LENGTH] = {0}; \
        struct timeval time; \
        gettimeofday(&time, NULL); \
        struct tm* now = localtime(&time.tv_sec); \
        snprintf(timestamp, TIMESTAMP_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d:%03d", \
                now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,\
                now->tm_hour, now->tm_min, now->tm_sec, (int)time.tv_usec/1000); \
        snprintf(szCoreLog, SINGLE_LOG_LENGTH, "%c  [%s][%s:%d]:" fmt"\n", LOG_VERBOSE, strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        snprintf(szLog, SINGLE_LOG_LENGTH, "%s  %5d  %5d  %s", timestamp, getpid(), (int)gettid(), szCoreLog); \
        slog2f(slog_handle, FILTER_CODE, SLOG2_INFO, szCoreLog); \
        Logger::getInstance().write(LOG_LVL_V, szLog, strlen(szLog));}while(0);
#endif

#ifndef SCREEN_LOGD
#define SCREEN_LOGD(fmt, ...) do{ \
        if (!Logger::getInstance().checkLogOutput(LOG_LVL_D)) { \
                break; \
        } \
        char szLog[SINGLE_LOG_LENGTH] = {0}; \
        char szCoreLog[SINGLE_LOG_LENGTH] = {0}; \
        char timestamp[TIMESTAMP_LENGTH] = {0}; \
        struct timeval time; \
        gettimeofday(&time, NULL); \
        struct tm* now = localtime(&time.tv_sec); \
        snprintf(timestamp, TIMESTAMP_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d:%03d", \
                now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,\
                now->tm_hour, now->tm_min, now->tm_sec, (int)time.tv_usec/1000); \
        snprintf(szCoreLog, SINGLE_LOG_LENGTH, "%c  [%s][%s:%d]:" fmt"\n", LOG_DEBUG, strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        snprintf(szLog, SINGLE_LOG_LENGTH, "%s  %5d  %5d  %s", timestamp, getpid(), (int)gettid(), szCoreLog); \
        slog2f(slog_handle, FILTER_CODE, SLOG2_INFO, szCoreLog); \
        Logger::getInstance().write(LOG_LVL_D, szLog, strlen(szLog));}while(0);
#endif

#ifndef SCREEN_LOGI
#define SCREEN_LOGI(fmt, ...) do{ \
        if (!Logger::getInstance().checkLogOutput(LOG_LVL_I)) { \
                break; \
        } \
        char szLog[SINGLE_LOG_LENGTH] = {0}; \
        char szCoreLog[SINGLE_LOG_LENGTH] = {0}; \
        char timestamp[TIMESTAMP_LENGTH] = {0}; \
        struct timeval time; \
        gettimeofday(&time, NULL); \
        struct tm* now = localtime(&time.tv_sec); \
        snprintf(timestamp, TIMESTAMP_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d:%03d", \
                now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,\
                now->tm_hour, now->tm_min, now->tm_sec, (int)time.tv_usec/1000); \
        snprintf(szCoreLog, SINGLE_LOG_LENGTH, "%c  [%s][%s:%d]:" fmt"\n", LOG_INFO, strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        snprintf(szLog, SINGLE_LOG_LENGTH, "%s  %5d  %5d  %s", timestamp, getpid(), (int)gettid(), szCoreLog); \
        slog2f(slog_handle, FILTER_CODE, SLOG2_INFO, szCoreLog); \
        Logger::getInstance().write(LOG_LVL_I, szLog, strlen(szLog));}while(0);
#endif

#ifndef SCREEN_LOGW
#define SCREEN_LOGW(fmt, ...) do{ \
        if (!Logger::getInstance().checkLogOutput(LOG_LVL_W)) { \
                break; \
        } \
        char szLog[SINGLE_LOG_LENGTH] = {0}; \
        char szCoreLog[SINGLE_LOG_LENGTH] = {0}; \
        char timestamp[TIMESTAMP_LENGTH] = {0}; \
        struct timeval time; \
        gettimeofday(&time, NULL); \
        struct tm* now = localtime(&time.tv_sec); \
        snprintf(timestamp, TIMESTAMP_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d:%03d", \
                now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,\
                now->tm_hour, now->tm_min, now->tm_sec, (int)time.tv_usec/1000); \
        snprintf(szCoreLog, SINGLE_LOG_LENGTH, "%c  [%s][%s:%d]:" fmt"\n", LOG_WARN, strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        snprintf(szLog, SINGLE_LOG_LENGTH, "%s  %5d  %5d  %s", timestamp, getpid(), (int)gettid(), szCoreLog); \
        slog2f(slog_handle, FILTER_CODE, SLOG2_WARNING, szCoreLog); \
        Logger::getInstance().write(LOG_LVL_W, szLog, strlen(szLog));}while(0);
#endif

#ifndef SCREEN_LOGE
#define SCREEN_LOGE(fmt, ...) do{ \
        if (!Logger::getInstance().checkLogOutput(LOG_LVL_E)) { \
                break; \
        } \
        char szLog[SINGLE_LOG_LENGTH] = {0}; \
        char szCoreLog[SINGLE_LOG_LENGTH] = {0}; \
        char timestamp[TIMESTAMP_LENGTH] = {0}; \
        struct timeval time; \
        gettimeofday(&time, NULL); \
        struct tm* now = localtime(&time.tv_sec); \
        snprintf(timestamp, TIMESTAMP_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d:%03d", \
                now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,\
                now->tm_hour, now->tm_min, now->tm_sec, (int)time.tv_usec/1000); \
        snprintf(szCoreLog, SINGLE_LOG_LENGTH, "%c  [%s][%s:%d]:" fmt"\n", LOG_ERROR, strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        snprintf(szLog, SINGLE_LOG_LENGTH, "%s  %5d  %5d  %s", timestamp, getpid(), (int)gettid(), szCoreLog); \
        slog2f(slog_handle, FILTER_CODE, SLOG2_ERROR, szCoreLog); \
        Logger::getInstance().write(LOG_LVL_E, szLog, strlen(szLog));}while(0);
#endif

class Logger {
public:
    static Logger& getInstance();

    virtual ~Logger(){}

    void setLogOutputStandard(uint32_t standard);

    void setMaxSize(uint32_t size);

    void setSlog();

    void write(const int level, const char* data, const int length);

    bool checkLogOutput(const int level);

private:
    Logger(const Logger& other) = delete;
    Logger(){
        mLogOutputStandard_ = LOG_LVL_D;
    }
    Logger& operator = (const Logger&) = delete;

    int mLogOutputStandard_;

    bool open();

    bool close();

    bool updateCurrentFile();

    bool fileFullHandling();

    bool recurMkdir(const std::string& dir);

    uint32_t size_;
    int fp_;
    uint32_t fileId_;
    std::string fileName_;
};
#endif //Logger_H
