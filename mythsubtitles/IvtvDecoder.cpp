
#include <string.h>
#include <sys/select.h>

#include "IvtvDecoder.h"
#include "vbi.h"

#define IVTV_SLICED_TYPE_TELETEXT       0x1     // Teletext (uses lines 6-22 for PAL)
#define IVTV_SLICED_TYPE_CC             0x4     // Closed Captions (line 21 NTSC)
#define IVTV_SLICED_TYPE_WSS            0x5     // Wide Screen Signal (line 23 PAL)
#define IVTV_SLICED_TYPE_VPS            0x7     // Video Programming System (PAL) (line 16)

#define FUDGE_FACTOR    (double)1.0

typedef union
{
    uint32_t words[2];
    uint64_t llword;
    fd_set   bits;
} ScanlineMask;

IvtvDecoder::IvtvDecoder(AVFormatContext* f, int vStream, int iStream)
    : Decoder(f)
{
    videoStream = vStream;
    ivtvStream = iStream;
    if (fudgeFactor == 0)
        fudgeFactor = FUDGE_FACTOR;
}

IvtvDecoder::~IvtvDecoder()
{
}

int
IvtvDecoder::decode()
{
    AVPacket pkt;
    while (av_read_frame(fmtCtx, &pkt) >= 0)
    {
        if (pkt.stream_index == videoStream)
            processVideoPacket(pkt);
        else if (pkt.stream_index == ivtvStream)
            processVBIPacket(pkt);
        av_free_packet(&pkt);
    }
    return 0;
}

void
IvtvDecoder::processVideoPacket(AVPacket& pkt)
{
    if (verbose > 2)
    {
        fprintf(fplog, "pos=%lld pts=%lld dts=%lld video\n",
            (long long)pkt.pos, (long long)pkt.pts, (long long)pkt.dts);
    }
    vbi->checkTime(pkt.dts);
}

void
IvtvDecoder::processVBIPacket(AVPacket& pkt)
{
    if (verbose > 1)
        fprintf(fplog, "pos=%lld pts=%lld dts=%lld vbi\n",
            (long long)pkt.pos, (long long)pkt.pts, (long long)pkt.dts);
    unsigned char* buf = pkt.data;
    int len = pkt.size;
    ScanlineMask mask;
    unsigned char* ptr = buf + 3; // assume we'll find magic cookie
    int i;
    vbi_sliced sliced_array[36];
    vbi_sliced* sliced = sliced_array;
    vbi_sliced* sliced_end;

    // Look for one of the magic cookies identifying this buffer
    // as VBI data captured by the ivtv driver.
    if (memcmp(buf, "tv0", 3) == 0)
    { 
        memcpy(&mask, ptr, 8);
        ptr += 8;
    }
    else if (memcmp(buf, "TV0", 3) == 0)
    {
        mask.words[0] = 0xffffffff;
        mask.words[1] = 0xf;
    }
    else
        return;

    if (verbose > 1)
        fprintf(fplog, "VBI: len=%d mask=%#llx\n", len, (unsigned long long)mask.llword);

    for (i = 0; i < 36; i++)
    {
        if (FD_ISSET(i, &mask.bits))
        {
            // We'll adjust the value of line later when we know the type
            sliced->line = i;
            sliced++;
            FD_CLR(i, &mask.bits);
        }
        // Optimization, skip to next word when this word is zero
        if (i < 32 && mask.words[0] == 0)
            i = 31;
        if (i >= 32 && mask.words[1] == 0)
            break;
    }
    sliced_end = sliced;

    sliced = sliced_array;
    for (unsigned char* end = buf + len; ptr+43 <= end; ptr += 43, sliced++)
    {
        if (sliced == sliced_end)
        {
            if (verbose > 1)
                fprintf(fplog, "Have more scanlines than bits in mask!\n");
            break;
        }
        int type = *ptr;
        // If line is < 18, it must be a field 1 scanline, just add 6
        // Otherwise, it depends on ivtv type
        if (sliced->line < 18)
            sliced->line += 6;
        else
        {
            if (type == IVTV_SLICED_TYPE_CC) // assume NTSC 525 scanlines
            {
                // 263 = 525/2+1
                sliced->line += (263-18+6);
            }
            else // assume PAL 625 scanlines
            {
                // 313 = 625/2+1
                sliced->line += (313-18+6);
            }
        }
        // set sliced->id based on type
        switch (type)
        {
        // Teletext (uses lines 6-22 for PAL)
        case IVTV_SLICED_TYPE_TELETEXT:
            sliced->id = VBI_SLICED_TELETEXT_B;
            break;

        // Closed Captions (line 21 NTSC)
        case IVTV_SLICED_TYPE_CC:
            sliced->id = VBI_SLICED_CAPTION_525;
            break;

        // Wide Screen Signal (line 23 PAL)
        case IVTV_SLICED_TYPE_WSS:
            sliced->id = VBI_SLICED_WSS_625; // VBI_SLICED_WSS_CPR1204?
            break;

        // Video Programming System (PAL) (line 16)
        case IVTV_SLICED_TYPE_VPS:
            sliced->id = VBI_SLICED_VPS;
            break;
        }

        // Copy the ivtv data to the vbi_sliced structure
        memcpy(sliced->data, ptr+1, 42);

        if (verbose > 1)
        {
            fprintf(fplog, "   ivtv-type=%#x line=%d zvbi-id=%#x", type, sliced->line, sliced->id);
            int n = (type == IVTV_SLICED_TYPE_CC) ? 2 : 42;
            logBytes(ptr+1, n);
        }
    }
    if (verbose && sliced != sliced_end)
    {
        fprintf(fplog, "Have fewer scanlines than bits in mask!\n");
    }

    if (verbose > 1)
        fflush(fplog);

    vbi->decode(sliced_array, sliced - sliced_array);
}
