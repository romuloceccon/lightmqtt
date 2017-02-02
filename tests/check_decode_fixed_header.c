#include <check.h>

#include "check_lightmqtt.h"
#include "../src/lightmqtt.c"

START_TEST(should_decode_fixed_header_valid_first_byte)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, res);
    ck_assert_int_eq(LMQTT_TYPE_CONNACK, header.type);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_first_byte)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x21);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_decode_fixed_header_single_byte_remaining_len)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x20);
    res = decode_fixed_header(&header, 1);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_four_byte_remaining_len)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0xff));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0xff));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0xff));

    res = decode_fixed_header(&header, 0x7f);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(268435455, header.remaining_length);
}
END_TEST

START_TEST(should_decode_fixed_header_invalid_fourth_byte)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));

    res = decode_fixed_header(&header, 0x80);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, header.remaining_length);
}
END_TEST

START_TEST(should_not_decode_after_remaining_length)
{
    int res;
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    res = decode_fixed_header(&header, 0x20);
    res = decode_fixed_header(&header, 1);
    res = decode_fixed_header(&header, 0);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, header.remaining_length);
}
END_TEST

START_TEST(should_not_decode_after_error)
{
    LMqttFixedHeader header;
    memset(&header, 0, sizeof(header));

    decode_fixed_header(&header, 0x20);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_AGAIN, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, decode_fixed_header(&header, 0x80));
    ck_assert_int_eq(LMQTT_DECODE_ERROR, decode_fixed_header(&header, 1));
}
END_TEST

TCase *tcase_decode_fixed_header(void)
{
    TCase *result = tcase_create("Decode fixed header");

    tcase_add_test(result, should_decode_fixed_header_valid_first_byte);
    tcase_add_test(result, should_decode_fixed_header_invalid_first_byte);
    tcase_add_test(result, should_decode_fixed_header_single_byte_remaining_len);
    tcase_add_test(result, should_decode_fixed_header_four_byte_remaining_len);
    tcase_add_test(result, should_decode_fixed_header_invalid_fourth_byte);
    tcase_add_test(result, should_not_decode_after_remaining_length);
    tcase_add_test(result, should_not_decode_after_error);

    return result;
}
