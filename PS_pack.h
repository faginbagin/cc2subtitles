#ifndef PS_PACK_H
#define PS_PACK_H

#include <stdio.h>
#include <vector>
#include "stream_id.h"
#include "mpg2util.h"

using namespace std;

/*
Program System Target Decoder parameters
*/
class P_STD_params
{
    public:
        P_STD_params();
        P_STD_params(stream_id_t id);
        P_STD_params(const P_STD_params&);

        P_STD_params& operator=(const P_STD_params&);

        int     decode(FILE* fp);
        void    print(FILE* fp);

/*
        stream_id   8   uimsbf
        '11'    2   bslbf
        P-STD_buffer_bound_scale    1   bslbf
        P-STD_buffer_size_bound 13  uimsbf
*/
        unsigned char   stream_id;
        unsigned char   buffer_bound_scale;
        unsigned short  buffer_size_bound;
};

/*
Program Stream pack
*/
class PS_pack
{
    public:
        PS_pack();
        ~PS_pack();

        // Reinitialize data members so this instance can be reused
        void    reset();

        int     decode(FILE* fp);
        void    print(FILE* fp);

/*
    pack_start_code 32  bslbf (00 00 01 ba)
*/
        unsigned char   stream_id;

        // Mark where this object starts in the input
        off_t   startPos;

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
        SystemClock system_clock_reference;

/*
    program_mux_rate    22  uimsbf
    marker_bit  1   bslbf
    marker_bit  1   bslbf
*/
        unsigned int program_mux_rate;

/*
    reserved    5   bslbf
    pack_stuffing_length    3   uimsbf
*/
        unsigned char pack_stuffing_length;

/*
    for (i=0;i<pack_stuffing_length;i++) {      
        stuffing_byte   8   bslbf
    }       
    if (nextbits() == system_header_start_code)  {      
        system_header ()        
    }       
*/

/*
    system_header_start_code    32  bslbf
    header_length   16  uimsbf
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

        // If start_code == system_header_start_code
        // then we have decoded a system header
        unsigned char start_code;
        unsigned short header_length;
        unsigned int rate_bound;
        unsigned int audio_bound;
        unsigned char fixed_flag;
        unsigned char CSPS_flag;
        unsigned char system_audio_lock_flag;
        unsigned char system_video_lock_flag;
        unsigned char video_bound;
        unsigned char packet_rate_restriction_flag;

/*
    while (nextbits () == '1') {        
        stream_id   8   uimsbf
        '11'    2   bslbf
        P-STD_buffer_bound_scale    1   bslbf
        P-STD_buffer_size_bound 13  uimsbf
    }       
*/
        vector<P_STD_params> params;

    private:
        void init();
};

#endif /* PS_PACK_H */

