
#include <string.h>
#include <sys/types.h>

#include "mpg2util.h"
#include "stream_id.h"
#include "PS_pack.h"

/*
Program System Target Decoder parameters
*/
P_STD_params::P_STD_params()
{
    stream_id = 0;
}

P_STD_params::P_STD_params(stream_id_t id)
{
    stream_id = id;
}

P_STD_params::P_STD_params(const P_STD_params& p)
{
    (*this) = p;
}

P_STD_params&
P_STD_params::operator=(const P_STD_params& p)
{
    stream_id = p.stream_id;
    buffer_bound_scale = p.buffer_bound_scale;
    buffer_size_bound = p.buffer_size_bound;
}

int
P_STD_params::decode(FILE* fp, bool detail)
{
    unsigned char buf[2];
/*
        stream_id   8   uimsbf
        '11'    2   bslbf
        P-STD_buffer_bound_scale    1   bslbf
        P-STD_buffer_size_bound 13  uimsbf
*/
    if (fread(&buf, 2, 1, fp) != 1)
        return EOF;
    // Validate the constant bits first
    if ((buf[0] & 0xc0) != 0xc0)
    {
        fprintf(stderr, "P_STD_params: pos=%lld: Expected 11xxxxxx (base2), found %#x\n",
            (long long)ftello(fp)-2, buf[0]);
        return EOF;
    }
    buffer_bound_scale = (buf[0] & 0x20) >> 5;
    buffer_size_bound = (buf[0] & 0x1f) << 8;
    buffer_size_bound += buf[1];
    return 0;
}

void
P_STD_params::print(FILE* fp)
{
    fprintf(fp, "P_STD_params: stream_id=%#x buffer_bound_scale=%d buffer_size_bound=%d\n",
        stream_id, buffer_bound_scale, buffer_size_bound);
}

/*
Program Stream pack
*/
PS_pack::PS_pack()
{
    init();
}

PS_pack::~PS_pack()
{
}

void
PS_pack::init()
{
    // Initialize all data members
    unsigned char* start = &stream_id;
    unsigned char* end = (&packet_rate_restriction_flag) + 1;
    memset(start, 0, end - start);
}

void
PS_pack::reset()
{
    init();
    // Empty params
    params.resize(0);
}

int
PS_pack::decode(FILE* fp, bool detail)
{
    int c;
    unsigned char buf[128];

/*
    pack_start_code 32  bslbf (00 00 01 ba)
*/
    if (stream_id != PS_pack_start_code)
    {
        // If someone else hasn't already decoded
        // a stream_id == PS_pack_start_code,
        // get it and validate it.
        if ((c = decodeStartCode(fp)) == EOF)
            return EOF;

        // Validate c
        if (c != PS_pack_start_code)
        {
            fprintf(stderr, "PS_pack: pos=%lld: Expected stream_id=%#x, found %#x\n",
                (long long)ftello(fp)-4, PS_pack_start_code, c);
            return 0-c;
        }
        stream_id = c;
    }
    startPos = ftello(fp)-4;

    if (detail)
    {
        // According to 13818.1, we need to decode the next 10 bytes
        if (fread(&buf, 10, 1, fp) != 1)
            return EOF;

    /*
        '01'    2   bslbf
        system_clock_reference_base [32..30]    3   bslbf
        marker_bit  1   bslbf
        system_clock_reference_base [29..15]    15  bslbf
        marker_bit  1   bslbf
        system_clock_reference_base [14..0] 15  bslbf
        marker_bit  1   bslbf
        system_clock_reference_extension    9   uimsbf
        marker_bit  1   bslbf
    */
        // The first six bytes should contain the system_clock parameters
        // buf[0] should contain (msb first)
        //  01
        //  xxx - system_clock_reference_base [32..30]
        //  1 - marker bit
        //  xx - system_clock_reference_base [29..28]
        // buf[1] should contain
        //  xxxx xxxx - system_clock_reference_base [27..20]
        // buf[2] should contain
        //  xxxx x - system_clock_reference_base [19..15]
        //  1 - marker bit
        //  xx - system_clock_reference_base [14..13]
        // buf[3] should contain
        //  xxxx xxxx - system_clock_reference_base [12..5]
        // buf[4] should contain
        //  xxxx x - system_clock_reference_base [4..0]
        //  1 - marker bit
        //  xx - system_clock_reference_extension [8..7]
        // buf[5] should contain
        //  xxxx xxx - system_clock_reference_extension [6..0]
        //  1 - marker bit

        // We'll let decodeClock do the decoding, we'll just validate the flags
        if ((buf[0] & 0xc0) != 0x40)
        {
            fprintf(stderr, "PS_pack: pos=%lld: SCR bits don't match expected constants\n",
                (long long)ftello(fp)-10);
            return EOF;
        }
        // Extract system_clock_reference
        if (decodeClock(buf, &system_clock_reference, ftello(fp)-10) == EOF)
            return EOF;

    /*
        program_mux_rate    22  uimsbf
        marker_bit  1   bslbf
        marker_bit  1   bslbf
    */
        // Next three bytes (buf[6..8]) contain program_mux rate and 2 marker bits
        // Validate the marker bits
        if ((buf[8] & 0x03) != 0x03)
        {
            fprintf(stderr, "PS_pack: pos=%lld: Expected 0x03 (marker bits) after program_mux_rate, found %#x\n",
                (long long)ftello(fp)-2, buf[8] & 0x03);
            return EOF;
        }
        program_mux_rate = (unsigned int)buf[6] << (16-2);
        program_mux_rate += (unsigned int)buf[7] << (8-2);
        program_mux_rate += (unsigned int)buf[8] >> 2;
    }
    else
    {
        // skip over 9 bytes and read the 10th so we can skip the stuffing bytes
        fseek(fp, 9L, SEEK_CUR);
        c = getc(fp);
        if (c == EOF)
            return EOF;
        buf[9] = c;
    }

/*
    reserved    5   bslbf
    pack_stuffing_length    3   uimsbf
*/
    pack_stuffing_length = buf[9] & 0x07;

/*
    for (i=0;i<pack_stuffing_length;i++) {      
        stuffing_byte   8   bslbf
    }       
*/
    // Skip over any stuffing bytes
    if (fseek(fp, (long)pack_stuffing_length, SEEK_CUR) < 0)
        return EOF;

/*
    if (nextbits() == system_header_start_code)  {      
        system_header ()        
    }       
*/

/*
    system_header_start_code    32  bslbf
    header_length   16  uimsbf
*/
    c = decodeStartCode(fp);
    if (c == EOF || c != system_header_start_code)
        return c;
    start_code = c;
    if (fread(buf, 2, 1, fp) != 1)
        return EOF;
    header_length = buf[0] << 8;
    header_length += buf[1];

    if (detail)
    {
    /*
        marker_bit  1   bslbf
        rate_bound  22  uimsbf
        marker_bit  1   bslbf
        audio_bound 6   uimsbf
        fixed_flag  1   bslbf
        CSPS_flag   1   bslbf
        system_audio_lock_flag  1   bslbf
        system_video_lock_flag  1   bslbf
        marker_bit  1   bslbf
        video_bound 5   uimsbf
        packet_rate_restriction_flag    1   bslbf
        reserved_byte   7   bslbf
    */
        // According to 13818.1, we need to decode the next 6 bytes
        if (fread(&buf, 6, 1, fp) != 1)
            return EOF;

        // Validate the constant bits first
        if ((buf[0] & 0x80) != 0x80 || (buf[2] & 0x01) != 0x01
         || (buf[4] & 0x20) != 0x20)
        {
            fprintf(stderr, "PS_pack: pos=%lld: System header bits don't match expected constants\n",
                (long long)ftello(fp)-6);
            return EOF;
        }
        // rate_bound[15..21]
        rate_bound = ((unsigned int)buf[0] & 0x7f) << 15;
        // rate_bound[7..14]
        rate_bound += (unsigned int)buf[1] << 7;
        // rate_bound[0..6]
        rate_bound += (unsigned int)buf[2] >> 1;

        audio_bound = buf[3] >> 2;
        fixed_flag = (buf[3] & 0x02) >> 1;
        CSPS_flag = buf[3] & 0x01;
        system_audio_lock_flag = (buf[4] & 0x80) >> 7;
        system_video_lock_flag = (buf[4] & 0x40) >> 6;
        video_bound = buf[4] & 0x1f;
        packet_rate_restriction_flag = (buf[5] & 0x80) >> 7;

    /*
        while (nextbits () == '1') {        
            stream_id   8   uimsbf
            '11'    2   bslbf
            P-STD_buffer_bound_scale    1   bslbf
            P-STD_buffer_size_bound 13  uimsbf
        }       

    };
    */
        while ((c = fgetc(fp)) != EOF && (c & 0x80))
        {
            P_STD_params p((stream_id_t)c);
            p.decode(fp, detail);
            params.push_back(p);
        }
        if (c != EOF)
        {
            ungetc(c, fp);
            c = 0;
        }
    }
    else
    {
        // seek over system header
        c = fseek(fp, header_length, SEEK_CUR);
    }
    return c;
}

#ifdef comment
static SystemClock prevSCR;
static char prevSCRbuf[6];
#endif

int
PS_pack::copy(FILE* fp, FILE* fpout)
{
    int c;
    unsigned char buf[10];

/*
    pack_start_code 32  bslbf (00 00 01 ba)
*/
    if (stream_id != PS_pack_start_code)
    {
        // If someone else hasn't already decoded
        // a stream_id == PS_pack_start_code,
        // get it and validate it.
        if ((c = decodeStartCode(fp)) == EOF)
            return EOF;

        // Validate c
        if (c != PS_pack_start_code)
        {
            fprintf(stderr, "PS_pack: pos=%lld: Expected stream_id=%#x, found %#x\n",
                (long long)ftello(fp)-4, PS_pack_start_code, c);
            return 0-c;
        }
        stream_id = c;
    }

    startPos = ftello(fpout);
    if (writeStartCode(stream_id, fpout) != 0)
        return EOF;

    // Copy the next 10 bytes so we know how many stuffing bytes to copy
    if (fread(buf, 10, 1, fp) != 1)
        return EOF;

#ifdef comment
    // Extract system_clock_reference
    if (decodeClock(buf, &system_clock_reference, ftello(fp)-10) == EOF)
        return EOF;
    // If it's less than the previous value, replace it.
    if (system_clock_reference.toSeconds() < prevSCR.toSeconds())
    {
        memcpy(buf, prevSCRbuf, sizeof(prevSCRbuf));
    }
    else
    {
        memcpy(prevSCRbuf, buf, sizeof(prevSCRbuf));
        prevSCR.base = system_clock_reference.base;
        prevSCR.extension = system_clock_reference.extension;
    }
#endif

    fwrite(buf, 10, 1, fpout);

/*
    reserved    5   bslbf
    pack_stuffing_length    3   uimsbf
*/
    pack_stuffing_length = buf[9] & 0x07;

/*
    for (i=0;i<pack_stuffing_length;i++) {      
        stuffing_byte   8   bslbf
    }       
*/
    // Copy any stuffing bytes
    if (pack_stuffing_length > 0)
    {
        if (fread(buf, pack_stuffing_length, 1, fp) != 1)
            return EOF;
        fwrite(buf, pack_stuffing_length, 1, fpout);
    }

/*
    if (nextbits() == system_header_start_code)  {      
        system_header ()        
    }       
*/

/*
    system_header_start_code    32  bslbf
    header_length   16  uimsbf
*/
    c = decodeStartCode(fp);
    if (c == EOF || c != system_header_start_code)
        return c;
    writeStartCode(c, fpout);

    if (fread(buf, 2, 1, fp) != 1)
        return EOF;
    header_length = buf[0] << 8;
    header_length += buf[1];
    fwrite(buf, 2, 1, fpout);

    // Copy system header
    return copyBytes(header_length, fp, fpout);
}

void
PS_pack::print(FILE* fp)
{
    fprintf(fp, "PS_pack: pos=%lld\n\
\tsystem_clock_reference=%.5f (base=%lld,extension=%d)\n\
\tprogram_mux_rate=%d\tpack_stuffing_length=%d\n",
        (long long)startPos, system_clock_reference.toSeconds(),
        system_clock_reference.base, system_clock_reference.extension,
        program_mux_rate,   pack_stuffing_length);

    if (start_code != 0)
    {
        fprintf(fp, "system_header: start_code=%#x header_length=%d\n\
\trate_bound=%d\taudio_bound=%d\tfixed_flag=%d\tCSPS_flag=%d\n\
\tsystem_audio_lock_flag=%d\tsystem_video_lock_flag=%d\n\
\tvideo_bound=%d\tpacket_rate_restriction_flag=%d\n",
            start_code,     header_length,  rate_bound,
            audio_bound,    fixed_flag,     CSPS_flag,
            system_audio_lock_flag,     system_video_lock_flag,
            video_bound,    packet_rate_restriction_flag);
    }
    for (int i = 0; i < params.size(); i++)
        params[i].print(fp);
}
