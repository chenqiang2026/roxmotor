#ifndef IVI_SCREEN_READER_H
#define IVI_SCREEN_READER_H

#include "ScreenReader.h"

class IVIScreenReader : public ScreenReader {
public:
    IVIScreenReader(int fd);
    ~IVIScreenReader() override;
    
    int readData(int curOpenScreenFd, char* buf, int len) override;
};

#endif // IVI_SCREEN_READER_H
