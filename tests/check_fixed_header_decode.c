#include "check_lightmqtt.h"

#include "../src/lmqtt_packet.c"

START_TEST(should_decode_fixed_header_valid_connack)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, res);
    ck_assert_int_eq(LMQTT_TYPE_CONNACK, header.type);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_connack)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x21);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_decode_fixed_header_pubrel)
{
    lmqtt_fixed_header_t header;

    memset(&header, 0, sizeof(header));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, fixed_header_decode(&header, 0x60));

    memset(&header, 0, sizeof(header));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x62));
    ck_assert_int_eq(LMQTT_TYPE_PUBREL, header.type);
}
END_TEST

START_TEST(should_decode_fixed_header_publish)
{
    lmqtt_fixed_header_t header;

    memset(&header, 0, sizeof(header));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, fixed_header_decode(&header, 0x3f));

    memset(&header, 0, sizeof(header));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x30));
    ck_assert_int_eq(LMQTT_TYPE_PUBLISH, header.type);
    ck_assert_int_eq(0, header.dup);
    ck_assert_int_eq(0, header.qos);
    ck_assert_int_eq(0, header.retain);

    memset(&header, 0, sizeof(header));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x3d));
    ck_assert_int_eq(LMQTT_TYPE_PUBLISH, header.type);
    ck_assert_int_eq(1, header.dup);
    ck_assert_int_eq(2, header.qos);
    ck_assert_int_eq(1, header.retain);
}
END_TEST

START_TEST(should_decode_fixed_header_single_byte_remaining_len)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20);
    res = fixed_header_decode(&header, 1);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_four_byte_remaining_len)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0xff));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0xff));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0xff));

    res = fixed_header_decode(&header, 0x7f);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(268435455, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_fourth_byte)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));

    res = fixed_header_decode(&header, 0x80);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_valid_zero_representation)
{
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, fixed_header_decode(&header, 0));
    ck_assert_int_eq(0, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_zero_representation)
{
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, fixed_header_decode(&header, 0));
}
END_TEST

START_TEST(should_not_decode_after_remaining_length)
{
    int res;
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    res = fixed_header_decode(&header, 0x20);
    res = fixed_header_decode(&header, 1);
    res = fixed_header_decode(&header, 0);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_not_decode_after_error)
{
    lmqtt_fixed_header_t header;
    memset(&header, 0, sizeof(header));

    fixed_header_decode(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, fixed_header_decode(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, fixed_header_decode(&header, 1));
}
END_TEST

START_TCASE("Decode fixed header")
{
    ADD_TEST(should_decode_fixed_header_valid_connack);
    ADD_TEST(should_decode_fixed_header_invalid_connack);
    ADD_TEST(should_decode_fixed_header_pubrel);
    ADD_TEST(should_decode_fixed_header_publish);
    ADD_TEST(should_decode_fixed_header_single_byte_remaining_len);
    ADD_TEST(should_decode_fixed_header_four_byte_remaining_len);
    ADD_TEST(should_decode_fixed_header_invalid_fourth_byte);
    ADD_TEST(should_decode_fixed_header_valid_zero_representation);
    ADD_TEST(should_decode_fixed_header_invalid_zero_representation);
    ADD_TEST(should_not_decode_after_remaining_length);
    ADD_TEST(should_not_decode_after_error);
}
END_TCASE
