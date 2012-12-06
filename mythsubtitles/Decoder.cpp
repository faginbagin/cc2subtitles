#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "IvtvDecoder.h"
#include "MpegDecoder.h"
#include "NuvDecoder.h"
#include "vbi.h"

Decoder::Decoder(AVFormatContext* f)
{
    fmtCtx = f;
    vbi = new VBIDecoder();
}

Decoder::~Decoder()
{
    if (fmtCtx)
        av_close_input_file(fmtCtx);
    delete vbi;
}

Decoder*
Decoder::GetDecoder()
{
    av_register_all();
    //av_log_set_level(AV_LOG_DEBUG);

    AVFormatContext* fmtCtx = 0;
    AVFormatParameters params;
    memset(&params, 0, sizeof(params));

    int ret;
    if ((ret = av_open_input_file(&fmtCtx, infile, (AVInputFormat*)0, 0, &params))!=0)
    {
        fprintf(stderr, "%s: av_open_input_file failed: %d,%s\n", infile, ret, strerror(errno));
        return 0;
    }

    fprintf(fplog, "Input format: %s,%s\n", fmtCtx->iformat->name,
        fmtCtx->iformat->long_name);

    if (av_find_stream_info(fmtCtx)<0)
    {
        fflush(fplog);
        fprintf(stderr, "%s: av_find_stream_info failed\n", infile);
        av_close_input_file(fmtCtx);
        return 0;
    }

    int videoStream = -1;
    int audioStream = -1;
    int ivtvStream = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++)
    {
        fprintf(fplog, "stream %d: codec_type=%s codec_id=%#x,%s\n",
            i, codec_type_string(fmtCtx->streams[i]->codec->codec_type),
            fmtCtx->streams[i]->codec->codec_id,
            codec_id_string(fmtCtx->streams[i]->codec->codec_id));
        switch (fmtCtx->streams[i]->codec->codec_type)
        {
            case CODEC_TYPE_VIDEO:
                if (videoStream < 0)
                    videoStream = i;
                break;
            case CODEC_TYPE_AUDIO:
                if (audioStream < 0)
                    audioStream = i;
                break;
            case CODEC_TYPE_DATA:
                // Check for ivtv VBI data
                if (ivtvStream < 0 && fmtCtx->streams[i]->codec->codec_id == CODEC_ID_MPEG2VBI)
                    ivtvStream = i;
                break;
        }
    }
    fprintf(fplog, "videoStream=%d audioStream=%d ivtvStream=%d\n", videoStream, audioStream, ivtvStream);

    if (videoStream >= 0)
    {
        if (ivtvStream >= 0)
            return new IvtvDecoder(fmtCtx, videoStream, ivtvStream);
        else if (strcmp(fmtCtx->iformat->name, "nuv") == 0)
            return new NuvDecoder(fmtCtx, videoStream);
        else if (strncmp(fmtCtx->iformat->name, "mpeg", 4) == 0)
            return new MpegDecoder(fmtCtx, videoStream, audioStream);
    }

    fflush(fplog);
    fprintf(stderr, "%s: Unsupported media format: %s\n", infile, fmtCtx->iformat->name);
    av_close_input_file(fmtCtx);
    return 0;
}

void
Decoder::logBytes(const uint8_t* ptr, int len)
{
    const uint8_t* end = ptr + len;
    for (; ptr < end; ptr++)
    {
        fprintf(fplog, " %02x", *ptr);
        int c = (*ptr) & 0x7f;
        if (0x20 <= c && c < 0x7f)
            fprintf(fplog, "(%c)", c);
    }
    putc('\n', fplog);
}

