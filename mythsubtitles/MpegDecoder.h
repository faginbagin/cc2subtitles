#ifndef MPEGDECODER_H
#define MPEGDECODER_H

#include "libzvbi.h"

#include "Decoder.h"

class MpegDecoder : public Decoder
{
    public:
        MpegDecoder(AVFormatContext* f, int vStream, int aStream);
        virtual ~MpegDecoder();

        virtual int decode();

    private:
        int videoStream;
        AVCodecContext* videoCtx;
        AVFrame* pic;
        int initVideo();
        int decodeVideo();

        AVPacket pkt;

        void DecodeDTVCC(const uint8_t* buf, uint buf_size, bool scte);
        void decodeDVBCC(const uint8_t* buf, int len);
        static void decodeDVDCC(AVCodecContext *s, const uint8_t *buf, int len);
        void decode_cc_dvd(const uint8_t *buf, int len);

        vbi_sliced* sliced_buf;
        uint sliced_buf_size;

        int audioStream;
        AVCodecContext* audioCtx;

        // Caption/Subtitle/Teletext decoders
        bool             choose_scte_cc;
        uint             invert_scte_field;
        uint             last_scte_field;

};

#endif /* MPEGDECODER_H */
