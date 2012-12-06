#ifndef STREAM_ID_H
#define STREAM_ID_H

typedef enum {
    MPEG_program_end_code = 0xb9,
    PS_pack_start_code = 0xba,
    system_header_start_code = 0xbb,
    program_stream_map = 0xbc,
    private_stream_1 = 0xbd,
    padding_stream = 0xbe,
    private_stream_2 = 0xbf,
    audio_stream_0 = 0xc0,
    audio_stream_31 = 0xdf,
    video_stream_0 = 0xe0,
    video_stream_15 = 0xef,
    ECM_stream = 0xf0,
    EMM_stream = 0xf1,
    DSMCC_stream = 0xf2,
    _13522_stream = 0xf3,
    H222_1_type_A = 0xf4,
    H222_1_type_B = 0xf5,
    H222_1_type_C = 0xf6,
    H222_1_type_D = 0xf7,
    H222_1_type_E = 0xf8,
    ancillary_stream = 0xf9,
    reserved_data_stream_0 = 0xfa,
    reserved_data_stream_4 = 0xfe,
    program_stream_directory = 0xff
} stream_id_t;

#endif /* STREAM_ID_H */
