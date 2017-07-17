#include "check_lightmqtt.h"

START_TEST(should_decode_fixed_header_invalid_packet_type)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x00, &error);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_TYPE,
        error);
}
END_TEST

START_TEST(should_decode_fixed_header_valid_connack)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20, &error);

    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(LMQTT_TYPE_CONNACK, header.type);
    ck_assert_int_eq(0, error);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_connack)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x21, &error);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_FLAGS,
        error);
}
END_TEST

START_TEST(should_decode_fixed_header_pubrel)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;

    memset(&header, 0, sizeof(header));
    res = fixed_header_decode(&header, 0x60, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);

    memset(&header, 0, sizeof(header));
    res = fixed_header_decode(&header, 0x62, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(LMQTT_TYPE_PUBREL, header.type);
}
END_TEST

START_TEST(should_decode_fixed_header_publish)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;

    memset(&header, 0, sizeof(header));
    res = fixed_header_decode(&header, 0x3f, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);

    memset(&header, 0, sizeof(header));
    res = fixed_header_decode(&header, 0x30, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(LMQTT_TYPE_PUBLISH, header.type);
    ck_assert_int_eq(0, header.dup);
    ck_assert_int_eq(LMQTT_QOS_0, header.qos);
    ck_assert_int_eq(0, header.retain);

    memset(&header, 0, sizeof(header));
    res = fixed_header_decode(&header, 0x3d, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    ck_assert_int_eq(LMQTT_TYPE_PUBLISH, header.type);
    ck_assert_int_eq(1, header.dup);
    ck_assert_int_eq(LMQTT_QOS_2, header.qos);
    ck_assert_int_eq(1, header.retain);
}
END_TEST

START_TEST(should_decode_fixed_header_single_byte_remaining_len)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20, &error);
    res = fixed_header_decode(&header, 1, &error);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_four_byte_remaining_len)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20, &error);

    res = fixed_header_decode(&header, 0xff, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0xff, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0xff, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);

    res = fixed_header_decode(&header, 0x7f, &error);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(268435455, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_fourth_byte)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20, &error);

    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);

    res = fixed_header_decode(&header, 0x80, &error);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, header.remaining_length);
    ck_assert_int_eq(LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_REMAINING_LENGTH,
        error);
}
END_TEST

START_TEST(should_decode_fixed_header_valid_zero_representation)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20, &error);

    res = fixed_header_decode(&header, 0, &error);
    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(0, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_zero_representation)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20, &error);

    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_decode_invalid_dup_flag)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x38, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_not_decode_after_remaining_length)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20, &error);
    res = fixed_header_decode(&header, 1, &error);
    res = fixed_header_decode(&header, 0, &error);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_not_decode_after_error)
{
    int res;
    lmqtt_error_t error = 0xcccc;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20, &error);

    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_CONTINUE, res);
    res = fixed_header_decode(&header, 0x80, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    res = fixed_header_decode(&header, 1, &error);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(LMQTT_ERROR_DECODE_FIXED_HEADER_INVALID_REMAINING_LENGTH,
        error);
}
END_TEST

START_TCASE("Decode fixed header")
{
    ADD_TEST(should_decode_fixed_header_invalid_packet_type);
    ADD_TEST(should_decode_fixed_header_valid_connack);
    ADD_TEST(should_decode_fixed_header_invalid_connack);
    ADD_TEST(should_decode_fixed_header_pubrel);
    ADD_TEST(should_decode_fixed_header_publish);
    ADD_TEST(should_decode_fixed_header_single_byte_remaining_len);
    ADD_TEST(should_decode_fixed_header_four_byte_remaining_len);
    ADD_TEST(should_decode_fixed_header_invalid_fourth_byte);
    ADD_TEST(should_decode_fixed_header_valid_zero_representation);
    ADD_TEST(should_decode_fixed_header_invalid_zero_representation);
    ADD_TEST(should_decode_invalid_dup_flag);
    ADD_TEST(should_not_decode_after_remaining_length);
    ADD_TEST(should_not_decode_after_error);
}
END_TCASE
