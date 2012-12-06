
#include <string.h>

#include "PES_packet.h"
#include "PS_pack.h"

PES_packet::PES_packet()
{
    stream_id = 0; 
    PES_packet_length = 0;
    init();
}

PES_packet::PES_packet(stream_id_t id)
{
    stream_id = id; 
    PES_packet_length = 0;
    init();
}

PES_packet::~PES_packet()
{
    delete PES_extension_field;
    delete PES_packet_data;
}

void
PES_packet::init()
{
    // Initialize all data members except stream_id and PES_packet_length to 0
    // PES_extension_field = 0;
    // PES_packet_data = 0;
    // pack_header = 0;
    unsigned char* start = (unsigned char*)&PES_packet_length_pos;
    unsigned char* end = (unsigned char*)(&PES_packet_data_length+1);
    memset(start, 0, end - start);
}

void
PES_packet::reset()
{
    // Delete any allocated buffers
    delete PES_extension_field;
    delete [] PES_packet_data;
    delete pack_header;
    // Reset key data members
    stream_id = 0; 
    PES_packet_length = 0;
    init();
}

int
PES_packet::decode(FILE* fp, bool detail)
{
    int c;
    unsigned char buf[128];
    off_t pos;

    if (stream_id == 0)
    {
        // Make sure we're starting at a start code prefix
        if ((c = decodeStartCode(fp)) == EOF)
            return EOF;

        stream_id = c;
        // Validate stream_id
        if (stream_id < program_stream_map)
        {
            fprintf(stderr, "PES_packet: pos=%lld: Invalid stream_id=%#x\n",
                (long long)ftello(fp)-4, stream_id);
            return -c;
        }
    }
    startPos = ftello(fp)-4;

    if (PES_packet_length == 0)
    {
        if (fread(buf, 2, 1, fp) != 1)
            return EOF;
        PES_packet_length = buf[0] << 8;
        PES_packet_length += buf[1];
    }
    PES_packet_length_pos = ftello(fp);

    if (detail)
    {
        if (stream_id != program_stream_map
            && stream_id != padding_stream
            && stream_id != private_stream_2
            && stream_id != ECM_stream
            && stream_id != EMM_stream
            && stream_id != program_stream_directory
            && stream_id != DSMCC_stream
            && stream_id != H222_1_type_E)
        {
            // We know we need to process three bytes, so get them
            if (fread(buf, 3, 1, fp) != 1)
                return EOF;
    /*
            '10'    2   bslbf
            PES_scrambling_control  2   bslbf
            PES_priority    1   bslbf
            data_alignment_indicator    1   bslbf
            copyright   1   bslbf
            original_or_copy    1   bslbf
    */
            c = buf[0];
            if ((c & 0xc0) != 0x80)
            {
                fprintf(stderr, "PES_packet: pos=%lld: Expected 10xxxxxx (base2), found %#x\n",
                    (long long)ftello(fp)-1, c);
                return EOF;
            }
            PES_scrambling_control = (c & 0x30) >> 4;
            PES_priority = (c & 0x8) >> 3;
            data_alignment_indicator = (c & 0x4) >> 2;
            copyright = (c & 0x2) >> 1;
            original_or_copy = (c & 0x1);

    /*
            PTS_DTS_flags   2   bslbf
            ESCR_flag   1   bslbf
            ES_rate_flag    1   bslbf
            DSM_trick_mode_flag 1   bslbf
            additional_copy_info_flag   1   bslbf
            PES_CRC_flag    1   bslbf
            PES_extension_flag  1   bslbf
    */
            c = buf[1];
            PTS_DTS_flags = (c & 0xc0) >> 6;
            if (PTS_DTS_flags == 01)
            {
                fprintf(stderr, "PES_packet: pos=%lld: Invalid PTS_DTS_flags, found %#x\n",
                    (long long)ftello(fp)-1, PTS_DTS_flags);
                return EOF;
            }
            ESCR_flag = (c & 0x20) >> 5;
            ES_rate_flag = (c & 0x10) >> 4;
            DSM_trick_mode_flag = (c & 0x8) >> 3;
            additional_copy_info_flag = (c & 0x4) >> 2;
            PES_CRC_flag = (c & 0x2) >> 1;
            PES_extension_flag = (c & 0x1);

    /*
            PES_header_data_length  8   uimsbf
    */
            c = buf[2];
            PES_header_data_length = c;
            PES_header_data_length_pos = ftello(fp);

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
            if (PTS_DTS_flags & 0x2)
            {
                if ((PTS.base = decodeTimeStamp(fp, PTS_DTS_flags)) == (unsigned long long)EOF)
                    return EOF;
            }
            if (PTS_DTS_flags & 0x1)
            {
                if ((DTS.base = decodeTimeStamp(fp, 0x01)) == (unsigned long long)EOF)
                    return EOF;
            }
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
            if (ESCR_flag)
            {
                if (fread(buf, 6, 1, fp) != 1)
                    return EOF;
                // Extract ESCR
                if (decodeClock(buf, &ESCR, ftello(fp)-10) == EOF)
                    return EOF;
            }

    /*
            if (ES_rate_flag == '1') {
                marker_bit  1   bslbf
                ES_rate 22  uimsbf
                marker_bit  1   bslbf
            }
     */
            if (ES_rate_flag)
            {
                if (fread(buf, 3, 1, fp) != 1)
                    return EOF;
                // Validate the marker bits first
                if ((buf[0] & 0x80) != 0x80 || (buf[2] & 0x01) != 0x01)
                {
                    fprintf(stderr, "PES_packet: pos=%lld: Marker bits in ES_rate are not 1\n",
                        (long long)ftello(fp)-3);
                    return EOF;
                }
                ES_rate = ((unsigned int)buf[0] & 0x7f)<< (16-1);
                ES_rate += (unsigned int)buf[1] << (8-1);
                ES_rate += (unsigned int)buf[2] >> 1;
            }

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
            if (DSM_trick_mode_flag)
            {
                c = getc(fp);
                if (c == EOF)
                    return EOF;
                trick_mode_control = (c & 0xc0) >> 5;
                if (trick_mode_control == fast_forward || trick_mode_control == fast_reverse)
                {
                    field_id = (c & 0x18) >> 3;
                    intra_slice_refresh = (c & 0x04) >> 2;
                    frequency_truncation = (c & 0x3);
                }
                else if (trick_mode_control == slow_motion || trick_mode_control == slow_reverse)
                {
                    rep_cntrl = (c & 0x1f);
                }
                else if (trick_mode_control == freeze_frame)
                {
                    field_id = (c & 0x18) >> 3;
                }
            }

    /*
            if ( additional_copy_info_flag == '1' ) {       
                marker_bit  1   bslbf
                additional_copy_info    7   bslbf
            }       
     */
            if (additional_copy_info_flag)
            {
                c = getc(fp);
                if (c == EOF)
                    return EOF;
                additional_copy_info = (c & 0x7f);
            }

    /*
            if ( PES_CRC_flag == '1' ) {        
                previous_PES_packet_CRC 16  bslbf
            }       
    */
            if (PES_CRC_flag)
            {
                if (fread(buf, 2, 1, fp) != 1)
                    return EOF;
                previous_PES_packet_CRC = buf[0] << 8;
                previous_PES_packet_CRC += buf[1];
            }

    /*
            if ( PES_extension_flag == '1' ) {      
                PES_private_data_flag   1   bslbf
                pack_header_field_flag  1   bslbf
                program_packet_sequence_counter_flag    1   bslbf
                P-STD_buffer_flag   1   bslbf
                reserved    3   bslbf
                PES_extension_flag_2    1   bslbf
    */
            if (PES_extension_flag)
            {
                c = getc(fp);
                if (c == EOF)
                    return EOF;
                PES_private_data_flag = (c & 0x80) >> 7;
                pack_header_field_flag = (c & 0x40) >> 6;
                program_packet_sequence_counter_flag = (c & 0x20) >> 5;
                P_STD_buffer_flag = (c & 0x10) >> 4;
                PES_extension_flag_2 = (c & 0x01);

    /*
                if ( PES_private_data_flag == '1' ) {       
                    PES_private_data    128 bslbf
                }       
    */
                if (PES_private_data_flag)
                {
                    if (fread(PES_private_data, 16, 1, fp) != 1)
                        return EOF;
                }

    /*
                if (pack_header_field_flag == '1' ) {       
                    pack_field_length   8   uimsbf
                    pack_header()       
                }       
    */
                if (pack_header_field_flag)
                {
                    c = getc(fp);
                    if (c == EOF)
                        return EOF;
                    pack_field_length = c;
                    pos = ftello(fp);
                    pack_header = new PS_pack();
                    if (pack_header->decode(fp, detail) == EOF)
                        return EOF;
                    if (ftello(fp) != pos + pack_field_length)
                    {
                        fprintf(stderr, "PES_packet: pos=%lld: expected pos=%lld after decoding PS_pack\n",
                            (long long)ftello(fp), (long long)pos + pack_field_length);
                    }
                }

    /*
                if(program_packet_sequence_counter_flag== '1'){     
                    marker_bit  1   bslbf
                    program_packet_sequence_counter 7   uimsbf
                    marker_bit  1   bslbf
                    MPEG1_MPEG2_identifier  1   bslbf
                    original_stuff_length   6   uimsbf
                }       
    */
                if (program_packet_sequence_counter_flag)
                {
                    if (fread(buf, 2, 1, fp) != 1)
                        return EOF;
                    program_packet_sequence_counter = buf[0] & 0x7f;
                    MPEG1_MPEG2_identifier = (buf[1] & 0x40) >> 6;
                    original_stuff_length = (buf[1] & 0x3f);
                }

    /*
                if ( P-STD_buffer_flag == '1' ) {       
                    '01'    2   bslbf
                    P-STD_buffer_scale  1   bslbf
                    P-STD_buffer_size   13  uimsbf
                }       
    */
                if (P_STD_buffer_flag)
                {
                    if (fread(buf, 2, 1, fp) != 1)
                        return EOF;
                    P_STD_buffer_scale = (buf[0] & 0x20) >> 5;
                    P_STD_buffer_size = (buf[0] & 0x1f) << 8;
                    P_STD_buffer_size += buf[1];
                }

    /*
                if ( PES_extension_flag_2 == '1'){      
                    marker_bit  1   bslbf
                    PES_extension_field_length  7   uimsbf
                    for(i=0;i<PES_extension_field_length;i++) {     
                        reserved    8   bslbf
                    }       
                }       
    */
                if (PES_extension_flag_2)
                {
                    c = getc(fp);
                    if (c == EOF)
                        return EOF;
                    PES_extension_field_length = c & 0x7f;
                    PES_extension_field = new unsigned char[PES_extension_field_length];
                    if (fread(PES_extension_field, PES_extension_field_length, 1, fp) != 1)
                        return EOF;
                }

            }
    /*
            }       
            for (i=0;i<N1;i++) {        
                stuffing_byte   8   bslbf
            }       
    */
            // Skip over stuffing bytes
            pos = ftello(fp);
            off_t stuffing_pos = PES_header_data_length_pos + PES_header_data_length;
            if (stuffing_pos < pos)
            {
                fprintf(stderr, "PES_packet: pos=%lld: (PES_header_data_length_pos+PES_header_data_length)=%lld < pos\n",
                    (long long)pos, (long long)stuffing_pos);
                return EOF;
            }
            if (fseeko(fp, stuffing_pos, SEEK_SET) < 0)
                return EOF;
    /*
            for (i=0;i<N2;i++) {        
                PES_packet_data_byte    8   bslbf
            }       
        }       
    */
        }

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
        pos = ftello(fp);
        off_t endpos = PES_packet_length_pos + PES_packet_length;
        PES_packet_data_length = (int)(endpos - pos);

        if (endpos < pos || PES_packet_data_length < 0)
        {
            fprintf(stderr, "PES_packet: pos=%lld: (PES_packet_length_pos+PES_packet_length)=%lld < pos\n",
                (long long)pos, (long long)endpos);
            return EOF;
        }

        PES_packet_data = new unsigned char[PES_packet_data_length];
        if (fread(PES_packet_data, PES_packet_data_length, 1, fp) != 1)
            return EOF;
    }
    else
    {
        // Just seek over the packet data
        fseek(fp, (long)PES_packet_length, SEEK_CUR);
    }
    return 0;
}

int
PES_packet::copy(FILE* fp, FILE* fpout)
{
    int c;
    unsigned char buf[128];
    off_t pos;

    if (stream_id == 0)
    {
        // Make sure we're starting at a start code prefix
        if ((c = decodeStartCode(fp)) == EOF)
            return EOF;

        stream_id = c;
        // Validate stream_id
        if (stream_id < program_stream_map)
        {
            fprintf(stderr, "PES_packet: pos=%lld: Invalid stream_id=%#x\n",
                (long long)ftello(fp)-4, stream_id);
            return -c;
        }
    }
    startPos = ftello(fp)-4;

    // Write the start code
    writeStartCode(stream_id, fpout);

    if (PES_packet_length == 0)
    {
        if (fread(buf, 2, 1, fp) != 1)
            return EOF;
        fwrite(buf, 2, 1, fpout);
        PES_packet_length = buf[0] << 8;
        PES_packet_length += buf[1];
    }

    return copyBytes(PES_packet_length, fp, fpout);
}

void
PES_packet::print(FILE* fp)
{
    fprintf(fp, "PES_packet: pos=%lld stream_id=%#x PES_packet_length=%d\n",
        (long long)startPos, stream_id, PES_packet_length);

    if (stream_id != program_stream_map
        && stream_id != padding_stream
        && stream_id != private_stream_2
        && stream_id != ECM_stream
        && stream_id != EMM_stream
        && stream_id != program_stream_directory
        && stream_id != DSMCC_stream
        && stream_id != H222_1_type_E)
    {
        fprintf(fp, "\
\tPES_scrambling_control=%d\tPES_priority=%d\tdata_alignment_indicator=%d\n\
\tcopyright=%d\toriginal_or_copy=%d\tPES_header_data_length=%d\n",
            PES_scrambling_control, PES_priority, data_alignment_indicator,
            copyright, original_or_copy, PES_header_data_length);

        if (PTS_DTS_flags)
        {
            if (PTS_DTS_flags & 0x2)
                fprintf(fp, "\tPTS=%.5f (%lld)", PTS.toSeconds(), PTS.base);
            if (PTS_DTS_flags & 0x1)
                fprintf(fp, "\tDTS=%.5f (%lld)", DTS.toSeconds(), DTS.base);
            fprintf(fp, "\n");
        }

        if (ESCR_flag)
        {
            fprintf(fp, "\tESCR: base=%lld\textension=%d\n",
                ESCR.base, ESCR.extension);
        }

        if (ES_rate_flag)
            fprintf(fp, "\tES_rate=%d\n", ES_rate);

        if (DSM_trick_mode_flag)
        {
            fprintf(fp, "\ttrick_mode_control=%d\n", trick_mode_control);

            if (trick_mode_control == fast_forward || trick_mode_control == fast_reverse || trick_mode_control == freeze_frame)
            {
                fprintf(fp, "\tfield_id=%d", field_id);
            }
            if (trick_mode_control == fast_forward || trick_mode_control == fast_reverse)
            {
                fprintf(fp, "\tintra_slice_refresh=%d\tfrequency_truncation=%d",
                    intra_slice_refresh, frequency_truncation);
            }
            if (trick_mode_control == slow_motion || trick_mode_control == slow_reverse)
            {
                fprintf(fp, "\trep_cntrl=%d", rep_cntrl);
            }
            fprintf(fp, "\n");
        }

        if (additional_copy_info_flag)
            fprintf(fp, "\tadditional_copy_info=%d\n", additional_copy_info);

        if (PES_CRC_flag)
            fprintf(fp, "\tprevious_PES_packet_CRC=%#x\n", previous_PES_packet_CRC);

        if (PES_extension_flag)
        {
            if (PES_private_data_flag)
                fprintf(fp, "\tPES_private_data=%.16s\n",
                    PES_private_data);

            if (pack_header_field_flag)
            {
                fprintf(fp, "\tpack_field_length=%d\tpack_header=%p\n",
                    pack_field_length, pack_header);
            }

            if (program_packet_sequence_counter_flag)
            {
                fprintf(fp,
"\tprogram_packet_sequence_counter=%d\tMPEG1_MPEG2_identifier=%d\n\
\toriginal_stuff_length=%d\n",
                    program_packet_sequence_counter, MPEG1_MPEG2_identifier,
                    original_stuff_length);
            }

            if (P_STD_buffer_flag)
            {
                fprintf(fp, "\tP_STD_buffer_scale=%d\tP_STD_buffer_size=%d\n",
                    P_STD_buffer_scale, P_STD_buffer_size);
            }

            if (PES_extension_flag_2)
            {
                fprintf(fp, "\tPES_extension_field_length=%d\tPES_extension_field=%p\n",
                    PES_extension_field_length, PES_extension_field);
            }
        }
    }

    fprintf(fp, "\tPES_packet_data_length=%d\tPES_packet_data=%p\n",
        PES_packet_data_length, PES_packet_data);
}

