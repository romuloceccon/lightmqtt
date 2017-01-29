#include <stdlib.h>
#include <check.h>

#include "../src/lightmqtt.c"

START_TEST(test_encode_simple_connect)
{
    LMqttConnect connect;
    u8 buf[256];
    int bytes_w = 0;
    int res;

    connect.client_id.len = 1;
    connect.client_id.buf = "X";
    memset(buf, 0, sizeof(buf));

    res = encode_connect_fixed_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(0x10, buf[0]);
    ck_assert_uint_eq(13,   buf[1]);

    ck_assert_uint_eq(0x00, buf[2]);
    ck_assert_uint_eq(0x04, buf[3]);
    ck_assert_uint_eq('M',  buf[4]);
    ck_assert_uint_eq('Q',  buf[5]);
    ck_assert_uint_eq('T',  buf[6]);
    ck_assert_uint_eq('T',  buf[7]);

    ck_assert_uint_eq(0x04,  buf[8]);
    ck_assert_uint_eq(0x00,  buf[9]);
    ck_assert_uint_eq(0x00,  buf[10]);
    ck_assert_uint_eq(0x00,  buf[11]);

    ck_assert_uint_eq(0x00,  buf[12]);
    ck_assert_uint_eq(1,     buf[13]);
    ck_assert_uint_eq('X',   buf[14]);

    ck_assert_uint_eq(0,     buf[15]);
    ck_assert_int_eq(15, bytes_w);
}
END_TEST

START_TEST(test_encode_connect_with_maximum_single_byte_remaining_length)
{
    LMqttConnect connect;
    u8 buf[256];
    int bytes_w = 0;
    int res;
    char client_id[256];

    memset(client_id, 'A', 115);
    connect.client_id.len = 115;
    connect.client_id.buf = client_id;
    memset(buf, 0, sizeof(buf));

    res = encode_connect_fixed_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(0x10, buf[0]);
    ck_assert_uint_eq(127,  buf[1]);

    ck_assert_uint_eq(0x00, buf[2]);
    ck_assert_uint_eq(0x04, buf[3]);
    ck_assert_uint_eq('M',  buf[4]);
    ck_assert_uint_eq('Q',  buf[5]);
    ck_assert_uint_eq('T',  buf[6]);
    ck_assert_uint_eq('T',  buf[7]);

    ck_assert_uint_eq(0x04,  buf[8]);
    ck_assert_uint_eq(0x00,  buf[9]);
    ck_assert_uint_eq(0x00,  buf[10]);
    ck_assert_uint_eq(0x00,  buf[11]);

    ck_assert_uint_eq(0x00,  buf[12]);
    ck_assert_uint_eq(115,   buf[13]);
    ck_assert_uint_eq('A',   buf[14]);
    ck_assert_uint_eq('A',   buf[128]);

    ck_assert_uint_eq(0,     buf[129]);
    ck_assert_int_eq(129, bytes_w);
}
END_TEST

START_TEST(test_encode_connect_with_minimum_two_byte_remaining_length)
{
    LMqttConnect connect;
    u8 buf[256];
    int bytes_w = 0;
    int res;
    char client_id[256];

    memset(client_id, 'B', 116);
    connect.client_id.len = 116;
    connect.client_id.buf = client_id;
    memset(buf, 0, sizeof(buf));

    res = encode_connect_fixed_header(&connect, 0, buf, sizeof(buf), &bytes_w);

    ck_assert_int_eq(LMQTT_ENCODE_FINISHED, res);

    ck_assert_uint_eq(0x10, buf[0]);
    ck_assert_uint_eq(0x80, buf[1]);
    ck_assert_uint_eq(1,    buf[2]);

    ck_assert_uint_eq(0x00, buf[3]);
    ck_assert_uint_eq(0x04, buf[4]);
    ck_assert_uint_eq('M',  buf[5]);
    ck_assert_uint_eq('Q',  buf[6]);
    ck_assert_uint_eq('T',  buf[7]);
    ck_assert_uint_eq('T',  buf[8]);

    ck_assert_uint_eq(0x04,  buf[9]);
    ck_assert_uint_eq(0x00,  buf[10]);
    ck_assert_uint_eq(0x00,  buf[11]);
    ck_assert_uint_eq(0x00,  buf[12]);

    ck_assert_uint_eq(0x00,  buf[13]);
    ck_assert_uint_eq(116,   buf[14]);
    ck_assert_uint_eq('B',   buf[15]);
    ck_assert_uint_eq('B',   buf[130]);

    ck_assert_uint_eq(0,     buf[131]);
    ck_assert_int_eq(131, bytes_w);
}
END_TEST

Suite* lightmqtt_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Light MQTT");

    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_encode_simple_connect);
    tcase_add_test(tc_core, test_encode_connect_with_maximum_single_byte_remaining_length);
    tcase_add_test(tc_core, test_encode_connect_with_minimum_two_byte_remaining_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = lightmqtt_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
