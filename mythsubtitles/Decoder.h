#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include "avformat.h"
}

extern int verbose;
extern const char* infile;
extern FILE* fplog;
extern double fudgeFactor;

class VBIDecoder;

class Decoder
{
    public:
        Decoder(AVFormatContext* f);
        virtual ~Decoder();

        virtual int decode() = 0;

        static Decoder* GetDecoder();
        static void logBytes(const uint8_t* ptr, int len);

    protected:
        AVFormatContext* fmtCtx;
        VBIDecoder* vbi;

};

#endif /* DECODER_H */
