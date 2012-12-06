#ifndef IVTVDECODER_H
#define IVTVDECODER_H

#include "Decoder.h"

class IvtvDecoder : public Decoder
{
    public:
        IvtvDecoder(AVFormatContext* f, int vStream, int iStream);
        virtual ~IvtvDecoder();

        virtual int decode();

    private:
        int videoStream;
        int ivtvStream;
        void processVideoPacket(AVPacket& pkt);
        void processVBIPacket(AVPacket& pkt);
};

#endif /* IVTVDECODER_H */
