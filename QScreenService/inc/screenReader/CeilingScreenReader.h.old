#ifndef CEILING_SCREEN_READER_H
#define CEILING_SCREEN_READER_H

#include "ScreenReader.h"

class CeilingScreenReader : public ScreenReader {
public:
    CeilingScreenReader(int fd);
    ~CeilingScreenReader() override;
    
    int readData(int curOpenScreenFd, char* buf, int len) override;
};

#endif // CEILING_SCREEN_READER_H
