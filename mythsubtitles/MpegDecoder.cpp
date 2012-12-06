
#include <string.h>

#include "MpegDecoder.h"
#include "vbi.h"

// For some reason, after mythtranscode & ffmpeg are used to convert mpegts
// files into dvd compatible mpeg2 files, the mpeg2 files run a little over
// 10 seconds/hour longer than the original.
// Without this fudge factor, the subtitles will lag behind the audio and video.
// This number was picked from averaging the changes
// seen as a result of transcoding 1 1080i QAM recording.
#define FUDGE_FACTOR    (double)1.0

MpegDecoder::MpegDecoder(AVFormatContext* f, int vStream, int aStream)
    : Decoder(f),
      // Closed Caption & Teletext decoders
      choose_scte_cc(true),
      invert_scte_field(0),
      last_scte_field(0)
{
    videoStream = vStream;
    audioStream = aStream;
    sliced_buf = 0;
    sliced_buf_size = 0;
    if (fudgeFactor == 0)
        fudgeFactor = FUDGE_FACTOR;
}

MpegDecoder::~MpegDecoder()
{
    av_free(pic);
    avcodec_close(videoCtx);
    delete[] sliced_buf;
}

int
MpegDecoder::decode()
{
    int ret;
    if ((ret = initVideo()) < 0)
        return ret;

    memset(&pkt, 0, sizeof(pkt));

    while (av_read_frame(fmtCtx, &pkt) >= 0)
    {
        if (pkt.stream_index == videoStream)
        {
            if ((ret = decodeVideo()) < 0)
                return -1;
        }
        av_free_packet(&pkt);
        memset(&pkt, 0, sizeof(pkt));
#ifdef TEST_VALGRIND
        if ((double)vbi->currTimestamp > 20.0)
            break;
#endif
    }

    return 0;
}

int
MpegDecoder::initVideo()
{
    videoCtx = fmtCtx->streams[videoStream]->codec;

    videoCtx->opaque = this;
    videoCtx->decode_cc_dvd = decodeDVDCC;
    videoCtx->get_buffer = avcodec_default_get_buffer;
    videoCtx->release_buffer = avcodec_default_release_buffer;
    videoCtx->draw_horiz_band = NULL;
    videoCtx->slice_flags = 0;
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
    videoCtx->error_resilience = FF_ER_COMPLIANT;
#else
    videoCtx->error_recognition = FF_ER_COMPLIANT;
#endif
    videoCtx->workaround_bugs = FF_BUG_AUTODETECT;
    videoCtx->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    videoCtx->idct_algo = FF_IDCT_AUTO;
    videoCtx->debug = 0;
    videoCtx->rate_emu = 0;
    videoCtx->error_rate = 0;

    AVCodec *codec = avcodec_find_decoder(videoCtx->codec_id);
    if (!codec)
    {
        fprintf(stderr, "%s: Unsupported codec\n", infile);
        return -1;
    }
    fprintf(fplog, "codec: id=%#x capabilities=%#x\n", codec->id, codec->capabilities);
#if 0
    if (codec->capabilities & CODEC_CAP_DR1)
    {
        videoCtx->flags |= CODEC_FLAG_EMU_EDGE;
    }
#endif

    if (avcodec_open(videoCtx, codec) < 0)
    {
        fflush(fplog);
        fprintf(stderr, "%s: av_open_codec failed\n", infile);
        return -1;
    }

    // Do we need to decode program map table to decode captions?
    fprintf(fplog, "fmtCtx->cur_pmt_sect=%p\n", fmtCtx->cur_pmt_sect);

    pic = avcodec_alloc_frame();

    return 0;
}

int
MpegDecoder::decodeVideo()
{
    int gotPic = 0;
    vbi->checkTime(pkt.dts);

    fflush(fplog);
    int ret = avcodec_decode_video(videoCtx, pic, &gotPic, pkt.data, pkt.size);
    fflush(stderr);

    if (ret < 0)
    {
        fprintf(stderr, "%s: avcodec_decode_video failed\n", infile);
        return -1;
    }
    if (verbose > 2)
    {
        fprintf(fplog, "pos=%lld dts=%lld size=%d ret=%d gotPic=%d\n",
            (long long)pkt.pos, (long long)pkt.dts, pkt.size, ret, gotPic);
    }
    if (gotPic)
    {
        if ((verbose > 1 && (pic->atsc_cc_len > 0 || pic->scte_cc_len > 0)) || pic->dvb_cc_len > 0)
            fprintf(fplog, "pos=%lld atsc_cc_len=%d scte_cc_len=%d dvb_cc_len=%d\n",
                (long long)pkt.pos, pic->atsc_cc_len, pic->scte_cc_len, pic->dvb_cc_len);


    uint cc_len;
    uint8_t *cc_buf;
    bool scte;

    // if both ATSC and SCTE caption data are available, choose ATSC(?)
    if (choose_scte_cc || pic->atsc_cc_len)
    {
        choose_scte_cc = false;
        cc_len = (uint)pic->atsc_cc_len;
        cc_buf = pic->atsc_cc_buf;
        scte = false;
    }
    else
    {
        cc_len = (uint)pic->scte_cc_len;
        cc_buf = pic->scte_cc_buf;
        scte = true;
    }


    for (uint i = 0; i < cc_len; i += ((cc_buf[i] & 0x1f) * 3) + 2)
    {
        DecodeDTVCC(cc_buf + i, cc_len - i, scte);
    }


        if (pic->dvb_cc_len > 0)
            decodeDVBCC(pic->dvb_cc_buf, pic->dvb_cc_len);
#if 0
                    // Decode DVB captions from MPEG user data
                    if (mpa_pic.dvb_cc_len > 0)
                    {
                        unsigned long long utc = lastccptsu;

                        for (uint i = 0; i < (uint)mpa_pic.dvb_cc_len; i += 2)
                        {
                            uint8_t cc_lo = mpa_pic.dvb_cc_buf[i];
                            uint8_t cc_hi = mpa_pic.dvb_cc_buf[i+1];

                            uint16_t cc_dt = (cc_hi << 8) | cc_lo;

                            if (cc608_good_parity(cc608_parity_table, cc_dt))
                            {
                                ccd608->FormatCCField(utc/1000, 0, cc_dt);
                                utc += 33367;
                            }
                        }
                        lastccptsu = utc;
                    }
#endif
    }
    return 0;
}

// Copied from mythtv/libs/libmythtv/avformatdecoder.cpp
// AvFormatDecoder::DecodeDTVCC
void
MpegDecoder::DecodeDTVCC(const uint8_t *buf, uint len, bool scte)
{
    if (!len)
        return;
    // closed caption data
    //cc_data() {
    // reserved                1 0.0   1  
    // process_cc_data_flag    1 0.1   bslbf 
    bool process_cc_data = buf[0] & 0x40;
    if (!process_cc_data)
    {
        if (verbose > 1)
            fprintf(fplog, "process_cc_data=0\n");
        return; // early exit if process_cc_data_flag false
    }

    // additional_data_flag    1 0.2   bslbf 
    //bool additional_data = buf[0] & 0x20;
    // cc_count                5 0.3   uimsbf 
    uint cc_count = buf[0] & 0x1f;
    // em_data                 8 1.0
    if (verbose > 1)
        fprintf(fplog, "len=%d scte=%d process_cc_data=%#x cc_count=%u\n", len, scte, process_cc_data, cc_count);

    if (len < 2+(3*cc_count))
        return;

    // bool had_608 = false, had_708 = false;

    if (sliced_buf_size < cc_count)
    {
        delete[] sliced_buf;
        sliced_buf = new vbi_sliced[cc_count];
        sliced_buf_size = cc_count;
    }
    vbi_sliced* sliced = sliced_buf;

    for (uint cur = 0; cur < cc_count; cur++)
    {
        uint cc_code  = buf[2+(cur*3)];
        bool cc_valid = cc_code & 0x04;
        if (!cc_valid)
        {
            if (verbose > 2)
                fprintf(fplog, "cc_code=%#x not valid\n", cc_code);
            continue;
        }

        uint data1    = buf[3+(cur*3)];
        uint data2    = buf[4+(cur*3)];
        uint cc_type  = cc_code & 0x03;
        uint field;

        if (scte || cc_type <= 0x1) // EIA-608 field-1/2
        {
            if (verbose > 1)
            {
                fprintf(fplog, "cc_code=%#x", cc_code);
                logBytes(buf+3+(cur*3), 2);
            }

            if (cc_type == 0x2)
            {
                // SCTE repeated field
                field = !last_scte_field;
                invert_scte_field = !invert_scte_field;
            }
            else
            {
                field = cc_type ^ invert_scte_field;
            }

            if (1) // (cc608_good_parity(cc608_parity_table, data))
            {
                // in film mode, we may start at the wrong field;
                // correct if XDS start/cont/end code is detected (must be field 2)
                if (scte && field == 0 &&
                    (data1 & 0x7f) <= 0x0f && (data1 & 0x7f) != 0x00)
                {
                    if (cc_type == 1)
                        invert_scte_field = 0;
                    field = 1;

                    // flush decoder
                    // ccd608->FormatCC(0, -1, -1);
                    fflush(fplog);
                    fprintf(stderr, "Flush decoder??? curr=%.4f\n", (double)vbi->currTimestamp);
                    fflush(stderr);
                }

                // had_608 = true;

                if (field == 0) // EIA-608 field 1, line 21
                    sliced->line = 21;
                else // EIA-608 field 2, line 284
                    sliced->line = 284;
                
                sliced->id = VBI_SLICED_CAPTION_525;
                sliced->data[0] = data1;
                sliced->data[1] = data2;
                sliced++;
                // ccd608->FormatCCField(lastccptsu / 1000, field, data);

                last_scte_field = field;
            }
        }
        else // EIA-708 not implemented
        {
            // had_708 = true;
            // ccd708->decode_cc_data(cc_type, data1, data2);
        }
    }
    if (sliced > sliced_buf)
        vbi->decode(sliced_buf, sliced - sliced_buf);
}

// Copied from mythtv/libs/libmythtv/avformatdecoder.cpp
// AvFormatDecoder::GetFrame
void
MpegDecoder::decodeDVBCC(const uint8_t *buf, int len)
{
    static int firstCall = 1;
    if (firstCall)
    {
        fflush(fplog);
        fprintf(stderr, "Untested: Decode DVB captions from MPEG user data!\n");
        fflush(stderr);
        firstCall = 0;
    }
#if 0
                    // Decode DVB captions from MPEG user data
                    if (mpa_pic.dvb_cc_len > 0)
                    {
                        unsigned long long utc = lastccptsu;

                        for (uint i = 0; i < (uint)mpa_pic.dvb_cc_len; i += 2)
                        {
                            uint8_t cc_lo = mpa_pic.dvb_cc_buf[i];
                            uint8_t cc_hi = mpa_pic.dvb_cc_buf[i+1];

                            uint16_t cc_dt = (cc_hi << 8) | cc_lo;

                            if (cc608_good_parity(cc608_parity_table, cc_dt))
                            {
                                ccd608->FormatCCField(utc/1000, 0, cc_dt);
                                utc += 33367;
                            }
                        }
                        lastccptsu = utc;
                    }
#endif
    if (sliced_buf_size < 1)
    {
        delete[] sliced_buf;
        sliced_buf = new vbi_sliced[1];
        sliced_buf_size = 1;
    }

    const uint8_t* ptr = buf;
    const uint8_t* end = buf + len;
    while (ptr < end)
    {
        // EIA-608 field 1, line 21
        sliced_buf->line = 21;
        sliced_buf->id = VBI_SLICED_CAPTION_525;
        memcpy(sliced_buf->data, ptr, 2);
        vbi->decode(sliced_buf, 1);
        ptr += 2;
    }
}

void
MpegDecoder::decodeDVDCC(AVCodecContext *s, const uint8_t *buf, int len)
{
    MpegDecoder* decoder = (MpegDecoder*)s->opaque;
    decoder->decode_cc_dvd(buf, len);
}

// Copied from mythtv/libs/libmythtv/avformatdecoder.cpp
// decode_cc_dvd
// which was taken from xine-lib libspucc by Christian Vogler
void
MpegDecoder::decode_cc_dvd(const uint8_t *buf, int len)
{
    if (verbose > 1)
    {
        fflush(stderr);
        fprintf(fplog, "decode_cc_dvd: frame_number=%d buf=%p len=%d\n",
            videoCtx->frame_number, buf, len);
    }

    if (sliced_buf_size < 2)
    {
        delete[] sliced_buf;
        sliced_buf = new vbi_sliced[2];
        sliced_buf_size = 2;
    }
#if 0
    // taken from xine-lib libspucc by Christian Vogler

    AvFormatDecoder *nd = (AvFormatDecoder *)(s->opaque);
    unsigned long long utc = nd->lastccptsu;

    const uint8_t *current = buf;
    int curbytes = 0;
    uint8_t data1, data2;
    uint8_t cc_code;
    int odd_offset = 1;

    while (curbytes < buf_size)
    {
        int skip = 2;

        cc_code = *current++;
        curbytes++;
    
        if (buf_size - curbytes < 2)
            break;
    
        data1 = *current;
        data2 = *(current + 1);
    
        switch (cc_code)
        {
            case 0xfe:
                /* expect 2 byte encoding (perhaps CC3, CC4?) */
                /* ignore for time being */
                skip = 2;
                break;

            case 0xff:
            {
                /* expect EIA-608 CC1/CC2 encoding */
                int tc = utc / 1000;
                int data = (data2 << 8) | data1;
                if (cc608_good_parity(nd->cc608_parity_table, data))
                    nd->ccd608->FormatCCField(tc, 0, data);
                utc += 33367;
                skip = 5;
                break;
            }

            case 0x00:
                /* This seems to be just padding */
                skip = 2;
                break;

            case 0x01:
                odd_offset = data2 & 0x80;
                if (odd_offset)
                    skip = 2;
                else
                    skip = 5;
                break;

            default:
                // rest is not needed?
                goto done;
                //skip = 2;
                //break;
        }
        current += skip;
        curbytes += skip;

    }
  done:
    nd->lastccptsu = utc;
#endif

    const uint8_t* ptr = buf;
    const uint8_t* end = buf + len - 2;

    while (ptr < end)
    {
        switch (*ptr)
        {
            case 0xfe:
                /* expect 2 byte encoding (perhaps CC3, CC4?) */
                /* ignore for time being */
                ptr += 3;
                break;

            case 0xff:
                /* expect EIA-608 CC1/CC2 encoding */
                if (verbose > 1)
                {
                    fprintf(fplog, "offset=%d cc_code=%#x", (int)(ptr-buf), *ptr);
                    logBytes(ptr+1, 5);
                }

                // seems to mark end of CCs when this byte is 0
                if (*(ptr+3) == 0xff)
                {
                    vbi_sliced* sliced = sliced_buf;

                    // EIA-608 field 1, line 21
                    sliced->line = 21;
                    sliced->id = VBI_SLICED_CAPTION_525;
                    memcpy(sliced->data, ptr+1, 2);
                    sliced++;

                    // EIA-608 field 2, line 284
                    // I'm guessing the second set of three bytes are for
                    // line 284 even though mythtv doesn't use them.
                    sliced->line = 284;
                    sliced->id = VBI_SLICED_CAPTION_525;
                    memcpy(sliced->data, ptr+4, 2);
                    sliced++;

                    vbi->decode(sliced_buf, sliced - sliced_buf);
                }
                ptr += 6;
                break;

            case 0x00:
                /* This seems to be just padding */
                ptr += 3;
                break;

            case 0x01:
                if (*(ptr+2) & 0x80)
                    ptr += 3;
                else
                    ptr += 6;
                break;

            default:
                // rest is not needed?
                goto done;
                //ptr += 3;
                //break;
        }
    }
done:
    fflush(fplog);
    return;
}

