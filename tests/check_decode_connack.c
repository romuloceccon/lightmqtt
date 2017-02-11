#include "check_lightmqtt.h"

#include "../src/lightmqtt.c"

START_TEST(should_decode_connack_valid_first_byte)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 1);

    ck_assert_int_eq(LMQTT_DECODE_AGAIN, res);
    ck_assert_int_eq(1, connack.session_present);
}
END_TEST

START_TEST(should_decode_connack_invalid_first_byte)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 3);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(0, connack.session_present);
}
END_TEST

START_TEST(should_decode_connack_valid_second_byte)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 1);
    res = decode_connack(&connack, 0);

    ck_assert_int_eq(LMQTT_DECODE_FINISHED, res);
    ck_assert_int_eq(1, connack.session_present);
    ck_assert_int_eq(0, connack.return_code);
}
END_TEST

START_TEST(should_decode_connack_invalid_second_byte)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 1);
    res = decode_connack(&connack, 6);

    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
    ck_assert_int_eq(1, connack.session_present);
    ck_assert_int_eq(0, connack.return_code);
}
END_TEST

START_TEST(should_not_decode_third_byte)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 1);
    res = decode_connack(&connack, 0);
    res = decode_connack(&connack, 0);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TEST(should_not_decode_after_error)
{
    int res;
    LMqttConnack connack;
    memset(&connack, 0, sizeof(connack));

    res = decode_connack(&connack, 2);
    res = decode_connack(&connack, 0);
    ck_assert_int_eq(LMQTT_DECODE_ERROR, res);
}
END_TEST

START_TCASE("Decode connack")
{
    ADD_TEST(should_decode_connack_valid_first_byte);
    ADD_TEST(should_decode_connack_invalid_first_byte);
    ADD_TEST(should_decode_connack_valid_second_byte);
    ADD_TEST(should_decode_connack_invalid_second_byte);
    ADD_TEST(should_not_decode_third_byte);
    ADD_TEST(should_not_decode_after_error);
}
END_TCASE
