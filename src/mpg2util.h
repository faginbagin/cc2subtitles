#ifndef MPG2UTIL_H
#define MPG2UTIL_H

#include <stdio.h>
#include <sys/types.h>

class PES_packet;

typedef struct
{
    unsigned long long base;    // 33 bits in 90 KHz units (27MHz/300)
    unsigned short extension;   // 9 bits in 27MHz units
    double toSeconds() { return ((double)(base*300 + extension))/27000000; }
} SystemClock;

class Timestamp
{
    public:
        unsigned long long base;    // 33 bits  in 90 KHz units (27MHz/300)
        double toSeconds() { return ((double)base)/90000; }
        Timestamp& operator=(const Timestamp& ts) { base = ts.base; }
        Timestamp& operator=(int ts) { base = (unsigned long long)ts; }
        operator double() { return ((double)base)/90000; }
};

int decodeStartCode(FILE* fp);
unsigned long long decodeTimeStamp(FILE* fp, unsigned char flags);
int decodeClock(unsigned char* buf, SystemClock* clock, off_t pos);

int writeStartCode(int c, FILE* fpout);
int copyBytes(int len, FILE* fpin, FILE* fpout);

#endif /* MPG2UTIL_H */
