#include "check_lightmqtt.h"

#include "../src/lmqtt_io.c"

START_TEST(should_get_integral_time_until_keep_alive)
{
    lmqtt_time_t time = { 10, 0 };
    long secs, nsecs;

    test_time_set(14, 0);
    ck_assert_int_eq(1, time_get_timeout_to(
        &time, test_time_get, 5, &secs, &nsecs));

    ck_assert_int_eq(1, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_fractional_time_until_keep_alive)
{
    lmqtt_time_t time = { 10, 500e6 };
    long secs, nsecs;

    test_time_set(14, 400e6);
    ck_assert_int_eq(1, time_get_timeout_to(
        &time, test_time_get, 5, &secs, &nsecs));

    ck_assert_int_eq(1, secs);
    ck_assert_int_eq(100e6, nsecs);
}
END_TEST

START_TEST(should_get_fractional_time_with_carry_until_keep_alive)
{
    lmqtt_time_t time = { 10, 500e6 };
    long secs, nsecs;

    test_time_set(14, 600e6);
    ck_assert_int_eq(1, time_get_timeout_to(
        &time, test_time_get, 5, &secs, &nsecs));

    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(900e6, nsecs);
}
END_TEST

START_TEST(should_get_time_until_expired_keep_alive)
{
    lmqtt_time_t time = { 10, 500e6 };
    long secs, nsecs;

    test_time_set(15, 600e6);
    ck_assert_int_eq(1, time_get_timeout_to(
        &time, test_time_get, 5, &secs, &nsecs));

    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_time_until_keep_alive_at_expiration_time)
{
    lmqtt_time_t time = { 10, 500e6 };
    long secs, nsecs;

    test_time_set(15, 500e6);
    ck_assert_int_eq(1, time_get_timeout_to(
        &time, test_time_get, 5, &secs, &nsecs));

    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_time_with_zeroed_keep_alive)
{
    lmqtt_time_t time = { 10, 0 };
    long secs, nsecs;

    test_time_set(11, 0);
    ck_assert_int_eq(0, time_get_timeout_to(
        &time, test_time_get, 0, &secs, &nsecs));

    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TCASE("Time")
{
    ADD_TEST(should_get_integral_time_until_keep_alive);
    ADD_TEST(should_get_fractional_time_until_keep_alive);
    ADD_TEST(should_get_fractional_time_with_carry_until_keep_alive);
    ADD_TEST(should_get_time_until_expired_keep_alive);
    ADD_TEST(should_get_time_until_keep_alive_at_expiration_time);
    ADD_TEST(should_get_time_with_zeroed_keep_alive);
}
END_TCASE
