#ifndef PES_PACKET_H
#define PES_PACKET_H

#include <stdio.h>
#include <sys/types.h>
#include "stream_id.h"
#include "mpg2util.h"

class PS_pack;

typedef enum
{
    fast_forward = 0,
    slow_motion = 1,
    freeze_frame = 2,
    fast_reverse = 3,
    slow_reverse = 4
} trick_mode_control_t;

class PES_packet
{
    public:
        PES_packet();
        PES_packet(stream_id_t id);
        ~PES_packet();

        // Reinitialize data members so this instance can be reused
        void    reset();   

        int     decode(FILE* fp, bool detail);
        int     copy(FILE* fp, FILE* fpout);
        void    print(FILE* fp);
/*
    packet_start_code_prefix    24  bslbf = 0000 0000 0001
    stream_id   8   uimsbf
    PES_packet_length   16  uimsbf
 */
        unsigned char   stream_id;
        unsigned short  PES_packet_length;
        off_t   PES_packet_length_pos;

        // Mark where this object starts in the input
        off_t   startPos;
/*
    if( stream_id != program_stream_map
    && stream_id != padding_stream
    && stream_id != private_stream_2
    && stream_id != ECM
    && stream_id != EMM
    && stream_id != program_stream_directory
    && stream_id != DSMCC_stream
    && stream_id != ITU-T Rec. H.222.1 type E_stream) {
        '10'    2   bslbf
        PES_scrambling_control  2   bslbf
        PES_priority    1   bslbf
        data_alignment_indicator    1   bslbf
        copyright   1   bslbf
        original_or_copy    1   bslbf
        PTS_DTS_flags   2   bslbf
        ESCR_flag   1   bslbf
        ES_rate_flag    1   bslbf
        DSM_trick_mode_flag 1   bslbf
        additional_copy_info_flag   1   bslbf
        PES_CRC_flag    1   bslbf
        PES_extension_flag  1   bslbf
        PES_header_data_length  8   uimsbf
 */
        unsigned char PES_scrambling_control;
        unsigned char PES_priority;
        unsigned char data_alignment_indicator;
        unsigned char copyright;
        unsigned char original_or_copy;
        unsigned char PTS_DTS_flags;
        unsigned char ESCR_flag;
        unsigned char ES_rate_flag;
        unsigned char DSM_trick_mode_flag;
        unsigned char additional_copy_info_flag;
        unsigned char PES_CRC_flag;
        unsigned char PES_extension_flag;
        unsigned char PES_header_data_length;
        off_t   PES_header_data_length_pos;

/*
        if (PTS_DTS_flags =='10' ) {        
            '0010'  4   bslbf
            PTS [32..30]    3   bslbf
            marker_bit  1   bslbf
            PTS [29..15]    15  bslbf
            marker_bit  1   bslbf
            PTS [14..0] 15  bslbf
            marker_bit  1   bslbf
        }       
        if (PTS_DTS_flags ==.11. )  {       
            '0011'  4   bslbf
            PTS [32..30]    3   bslbf
            marker_bit  1   bslbf
            PTS [29..15]    15  bslbf
            marker_bit  1   bslbf
            PTS [14..0] 15  bslbf
            marker_bit  1   bslbf
            '0001'  4   bslbf
            DTS [32..30]    3   bslbf
            marker_bit  1   bslbf
            DTS [29..15]    15  bslbf
            marker_bit  1   bslbf
            DTS [14..0] 15  bslbf
            marker_bit  1   bslbf
        }       
 */
        Timestamp PTS;  // 33 bit presentation timestamp
        Timestamp DTS;  // 33 bit decoding timestamp

/*
        if (ESCR_flag=='1') {       
            reserved    2   bslbf
            ESCR_base[32..30]   3   bslbf
            marker_bit  1   bslbf
            ESCR_base[29..15]   15  bslbf
            marker_bit  1   bslbf
            ESCR_base[14..0]    15  bslbf
            marker_bit  1   bslbf
            ESCR_extension  9   uimsbf
            marker_bit  1   bslbf
        }       
 */
        SystemClock ESCR;

/*
        if (ES_rate_flag == '1') {
            marker_bit  1   bslbf
            ES_rate 22  uimsbf
            marker_bit  1   bslbf
        }
 */
        unsigned int    ES_rate;    // 22 bits

/*
        if (DSM_trick_mode_flag == '1') {       
            trick_mode_control  3   uimsbf
            if ( trick_mode_control == fast_forward ) {     
                field_id    2   bslbf
                intra_slice_refresh 1   bslbf
                frequency_truncation    2   bslbf
            }       
            else if ( trick_mode_control == slow_motion ) {     
                rep_cntrl   5   uimsbf
            }       
            else if ( trick_mode_control == freeze_frame) {     
                field_id    2   uimsbf
                reserved    3   bslbf
            }       
            else if ( trick_mode_control == fast_reverse' ) {       
                field_id    2   bslbf
                intra_slice_refresh 1   bslbf
                frequency_truncation    2   bslbf
            else if ( trick_mode_control == slow_reverse ) {        
                rep_cntrl   5   uimsbf
            }       
            else        
                reserved    5   bslbf
        }       
 */
        unsigned char trick_mode_control;   // 3 bits

        // For fast_forward, freeze_frame & fast_reverse
        unsigned char field_id;             // 2 bits

        // For fast_forward & fast_reverse
        unsigned char intra_slice_refresh;  // 1 bit
        unsigned char frequency_truncation; // 2 bits

        // For slow_motion & slow_reverse
        unsigned char rep_cntrl;            // 5 bits

/*
        if ( additional_copy_info_flag == '1' ) {       
            marker_bit  1   bslbf
            additional_copy_info    7   bslbf
        }       
 */
        unsigned char additional_copy_info; // 7 bits

/*
        if ( PES_CRC_flag == '1' ) {        
            previous_PES_packet_CRC 16  bslbf
        }       
*/
        unsigned short previous_PES_packet_CRC; // 16 bits

/*
        if ( PES_extension_flag == '1' ) {      
            PES_private_data_flag   1   bslbf
            pack_header_field_flag  1   bslbf
            program_packet_sequence_counter_flag    1   bslbf
            P-STD_buffer_flag   1   bslbf
            reserved    3   bslbf
            PES_extension_flag_2    1   bslbf
*/
        unsigned char PES_private_data_flag;                // 1 bit
        unsigned char pack_header_field_flag;               // 1 bit
        unsigned char program_packet_sequence_counter_flag; // 1 bit
        unsigned char P_STD_buffer_flag;                    // 1 bit
        unsigned char PES_extension_flag_2;                 // 1 bit

/*
            if ( PES_private_data_flag == '1' ) {       
                PES_private_data    128 bslbf
            }       
*/
        unsigned char PES_private_data[16]; // 128 bits, 16 bytes

/*
            if (pack_header_field_flag == '1' ) {       
                pack_field_length   8   uimsbf
                pack_header()       
            }       
*/
        unsigned char pack_field_length;    // 8 bits
        PS_pack* pack_header;

/*
            if(program_packet_sequence_counter_flag== '1'){     
                marker_bit  1   bslbf
                program_packet_sequence_counter 7   uimsbf
                marker_bit  1   bslbf
                MPEG1_MPEG2_identifier  1   bslbf
                original_stuff_length   6   uimsbf
            }       
*/
        unsigned char program_packet_sequence_counter;  // 7 bits
        unsigned char MPEG1_MPEG2_identifier;           // 1 bit
        unsigned char original_stuff_length;            // 6 bits

/*
            if ( P-STD_buffer_flag == '1' ) {       
                '01'    2   bslbf
                P-STD_buffer_scale  1   bslbf
                P-STD_buffer_size   13  uimsbf
            }       
*/
        unsigned char   P_STD_buffer_scale; // 1 bit
        unsigned short  P_STD_buffer_size;  // 13 bits

/*
            if ( PES_extension_flag_2 == '1'){      
                marker_bit  1   bslbf
                PES_extension_field_length  7   uimsbf
                for(i=0;i<PES_extension_field_length;i++) {     
                    reserved    8   bslbf
                }       
            }       
*/
        unsigned char   PES_extension_field_length;   // 7 bits
        unsigned char*  PES_extension_field;

/*
        }       
        for (i=0;i<N1;i++) {        
            stuffing_byte   8   bslbf
        }       
        for (i=0;i<N2;i++) {        
            PES_packet_data_byte    8   bslbf
        }       
    }       
*/
        unsigned char*  PES_packet_data;
        int  PES_packet_data_length;

/*
    else if ( stream_id == program_stream_map
    || stream_id == private_stream_2
    || stream_id == ECM
    || stream_id == EMM
    || stream_id == program_stream_directory
    || stream_id == DSMCC_stream)
    || stream_id == ITU-T Rec. H.222.1 type E stream {      
        for ( i=0;i<PES_packet_length;i++) {        
            PES_packet_data_byte    8   bslbf
        }       
    }       
    else if ( stream_id == padding_stream) {        
        for ( i=0;i<PES_packet_length;i++) {        
            padding_byte    8   bslbf
        }       
    }       
}       
*/

private:
    void    init();
};

#endif /* PES_PACKET_H */
