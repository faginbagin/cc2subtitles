#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "mpg2util.h"

#define SECONDS 5
static int caughtsignal = 0;

void
catchsignal(int i)
{
    caughtsignal = i;
}

int
decodeStartCode(FILE* fp)
{
    int c;
    int first = 1;

    while (!caughtsignal)
    {
        c = getc(fp);
        if (c == EOF)
            return c;
        if (c != 0)
        {
            if (first)
            {
                fprintf(stderr, "Warning: start_code_prefix not found at pos=%lld, will search for %d seconds\n",
                    (long long)ftello(fp)-1, SECONDS);
                first = 0;
                signal(SIGALRM, catchsignal);
                alarm(SECONDS);
            }
            while (c != 0)
            {
                c = getc(fp);
                if (c == EOF)
                    return c;
            }
        }

        // We have our first 0, look for the second
        c = getc(fp);
        if (c == EOF)
            return c;
        if (c != 0)
            continue;

        // We have our second 0, look for 1
        c = getc(fp);
        if (c == EOF)
            return c;
        if (c == 1)
            break;
    }
    if (caughtsignal)
    {
        fprintf(stderr, "Can't find start_code after %d seconds, pos=%lld, giving up\n",
            SECONDS, (long long)ftello(fp));
        alarm(0);
        caughtsignal = 0;
        return EOF;
    }
    c = getc(fp);
    if (!first)
    {
        // clear alarm
        alarm(0);
        caughtsignal = 0;
        fprintf(stderr, "Finally found start_code=%#x at pos=%lld\n", c, (long long)ftello(fp)-4);
    }
    return c;
}

// Read and decode timestamp
unsigned long long
decodeTimeStamp(FILE* fp, unsigned char flags)
{
    unsigned char buf[5];
/*
            '0010'  4   bslbf
            PTS [32..30]    3   bslbf
            marker_bit  1   bslbf
            PTS [29..15]    15  bslbf
            marker_bit  1   bslbf
            PTS [14..0] 15  bslbf
            marker_bit  1   bslbf
*/
    // The next five bytes should contain a timestamp
    if (fread(buf, 5, 1, fp) != 1)
        return (unsigned long long)EOF;

    // buf[0] should contain (msb first)
    //  00xx (flags)
    //  000 - timestamp [32..30]
    //  1 - marker bit
    // buf[1] should contain
    //  xxxx xxxx - timestamp [29..22]
    // buf[2] should contain
    //  xxxx xxx - timestamp [21..15]
    //  1 - marker bit
    // buf[3] should contain
    //  xxxx xxxx - timestamp [14..7]
    // buf[4] should contain
    //  xxxx xxx - timestamp [6..0]
    //  1 - marker bit

    // Validate the flags and marker bits first
    unsigned char decodeFlags = (buf[0] & 0xf0) >> 4;
    if (decodeFlags != flags)
    {
        fprintf(stderr, "PES_header: pos=%lld: Expected timestamp flags=%#x, found %#x\n",
            (long long)ftello(fp)-5, flags, decodeFlags);
        return (unsigned long long)EOF;
    }

    if ((buf[0] & 0x01) != 0x01 || (buf[2] & 0x01) != 0x01
     || (buf[4] & 0x01) != 0x01)
    {
        fprintf(stderr, "PES_header: pos=%lld: Marker bits in timestamp are not 1\n",
            (long long)ftello(fp)-5);
        return (unsigned long long)EOF;
    }

    unsigned long long timestamp = ((unsigned long long)buf[0] & 0x0e) << (30-1);
    timestamp += ((unsigned long long)buf[1]) << 22;
    timestamp += ((unsigned long long)buf[2] & 0xfe) << (15-1);
    timestamp += ((unsigned long long)buf[3]) << 7;
    timestamp += ((unsigned long long)buf[4] & 0xfe) >> 1;

    return timestamp;
}

int
decodeClock(unsigned char* buf, SystemClock* clock, off_t pos)
{
    // buf contains six bytes holding a clock reference value
    // buf[0] should contain (msb first)
    //  xx - flags validated by caller
    //  xxx - clock->base [32..30]
    //  1 - marker bit
    //  xx - clock->base [29..28]
    // buf[1] should contain
    //  xxxx xxxx - clock->base [27..20]
    // buf[2] should contain
    //  xxxx x - clock->base [19..15]
    //  1 - marker bit
    //  xx - clock->base [14..13]
    // buf[3] should contain
    //  xxxx xxxx - clock->base [12..5]
    // buf[4] should contain
    //  xxxx x - clock->base [4..0]
    //  1 - marker bit
    //  xx - clock->extension [8..7]
    // buf[5] should contain
    //  xxxx xxx - clock->extension [6..0]
    //  1 - marker bit

    // Validate the constant bits first
    if ((buf[0] & 0x04) != 0x04 || (buf[2] & 0x04) != 0x04
     || (buf[4] & 0x04) != 0x04 || (buf[5] & 0x01) != 0x01)
    {
        fprintf(stderr, "decodeClock: pos=%lld: Marker bits in clock are not 1\n",
            (long long)pos);
        return EOF;
    }

    // Extract clock->base
    clock->base = ((unsigned long long)buf[0] & 0x38) << (30-3);
    clock->base += ((unsigned long long)buf[0] & 0x03) << 28;
    clock->base += ((unsigned long long)buf[1]) << 20;
    clock->base += ((unsigned long long)buf[2] & 0xf8) << (15-3);
    clock->base += ((unsigned long long)buf[2] & 0x03) << 13;
    clock->base += ((unsigned long long)buf[3]) << 5;
    clock->base += ((unsigned long long)buf[4] & 0xf8) >> 3;
    // Exract clock->extension
    clock->extension = ((unsigned short)buf[4] & 0x03) << 7;
    clock->extension += ((unsigned short)buf[5] & 0xfe) >> 1;

    return 0;
}

int
writeStartCode(int c, FILE* fpout)
{
    unsigned char buf[4];
    buf[0] = buf[1] = 0;
    buf[2] = 1;
    buf[3] = c;
    if (fwrite(buf, 4, 1, fpout) != 1)
    {
        fprintf(stderr, "writeStartCode: write failed: %s\n", strerror(errno));
        return EOF;
    }
    return 0;
}

int
copyBytes(int len, FILE* fp, FILE* fpout)
{
    unsigned char buf[2048];

    while (len > 0)
    {
        int copy = (len > sizeof(buf) ? sizeof(buf) : len);
        int rd = fread(buf, 1, copy, fp);
        if (rd <= 0)
        {
            fprintf(stderr, "copyBytes: read failed: %s\n", strerror(errno));
            return EOF;
        }
        if (rd < copy)
        {
            fprintf(stderr, "copyBytes: read %d bytes, expected %d\n", rd, copy);
        }
        if (fwrite(buf, rd, 1, fpout) != 1)
        {
            fprintf(stderr, "copyBytes: write failed: %s\n", strerror(errno));
            return EOF;
        }
        if (rd < copy)
            break;

        len -= rd;
    }

    return 0;
}
