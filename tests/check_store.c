#include "check_lightmqtt.h"

#include "../src/lmqtt_store.c"

#define PREPARE \
    int data[LMQTT_STORE_SIZE]; \
    void *data_addr; \
    lmqtt_class_t class = -1; \
    int res; \
    lmqtt_store_t store; \
    long secs = -1, nsecs = -1; \
    do { \
        memset(&data, 0xcc, sizeof(data)); \
        memset(&store, 0, sizeof(store)); \
        store.get_time = &test_time_get; \
        store.timeout = 10; \
    } while(0)

START_TEST(should_get_id)
{
    PREPARE;

    ck_assert_uint_eq(0, lmqtt_store_get_id(&store));
    ck_assert_uint_eq(1, lmqtt_store_get_id(&store));
    ck_assert_uint_eq(2, lmqtt_store_get_id(&store));
}
END_TEST

START_TEST(should_append_one_object)
{
    PREPARE;

    res = lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, 1, &data[0]);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_CONNECT, class);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_append_multiple_objects)
{
    PREPARE;

    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[0], data_addr);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_not_pop_nonexistent_object)
{
    PREPARE;

    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, class);
    ck_assert_ptr_eq(NULL, data_addr);
}
END_TEST

START_TEST(should_not_append_object_if_store_is_full)
{
    int i, data_n;

    PREPARE;

    for (i = 0; i < LMQTT_STORE_SIZE; i++)
        lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, i, &data[i]);

    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, i, &data_n);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_pop_object)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_next(&store);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_get_second_object_after_popping_first_one)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    lmqtt_store_next(&store);
    lmqtt_store_next(&store);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_get_first_object_after_popping_second_one)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    lmqtt_store_next(&store);
    lmqtt_store_next(&store);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 2, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_pop_any_object)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);

    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[0], data_addr);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_not_peek_nonexistent_object)
{
    PREPARE;

    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, class);
    ck_assert_ptr_eq(NULL, data_addr);
}
END_TEST

START_TEST(should_peek_object)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);

    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_not_peek_first_object_after_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    res = lmqtt_store_next(&store);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_peek_second_object_after_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    res = lmqtt_store_next(&store);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_append_and_peek_after_next)
{
    PREPARE;

    res = lmqtt_store_next(&store);
    ck_assert_int_eq(0, res);

    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_not_pop_before_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_peek_object_after_pop)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    lmqtt_store_next(&store);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_peek_object_after_pop_any)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);

    res = lmqtt_store_pop_any(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, &class, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH, class);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_get_timeout_before_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);

    res = lmqtt_get_timeout(&store, &secs, &nsecs);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_timeout_after_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    test_time_set(10, 0);
    lmqtt_store_next(&store);

    test_time_set(11, 0);
    res = lmqtt_get_timeout(&store, &secs, &nsecs);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(9, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_closest_timeout_after_multiple_next)
{
    PREPARE;

    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 1, &data[0]);
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH, 2, &data[1]);
    test_time_set(10, 0);
    lmqtt_store_next(&store);
    test_time_set(11, 0);
    lmqtt_store_next(&store);

    test_time_set(12, 0);
    res = lmqtt_get_timeout(&store, &secs, &nsecs);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(8, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TCASE("Store")
{
    ADD_TEST(should_get_id);
    ADD_TEST(should_append_one_object);
    ADD_TEST(should_append_multiple_objects);
    ADD_TEST(should_not_pop_nonexistent_object);
    ADD_TEST(should_not_append_object_if_store_is_full);
    ADD_TEST(should_pop_object);
    ADD_TEST(should_get_second_object_after_popping_first_one);
    ADD_TEST(should_get_first_object_after_popping_second_one);
    ADD_TEST(should_pop_any_object);
    ADD_TEST(should_not_peek_nonexistent_object);
    ADD_TEST(should_peek_object);
    ADD_TEST(should_not_peek_first_object_after_next);
    ADD_TEST(should_peek_second_object_after_next);
    ADD_TEST(should_append_and_peek_after_next);
    ADD_TEST(should_not_pop_before_next);
    ADD_TEST(should_peek_object_after_pop);
    ADD_TEST(should_peek_object_after_pop_any);
    ADD_TEST(should_get_timeout_before_next);
    ADD_TEST(should_get_timeout_after_next);
    ADD_TEST(should_get_closest_timeout_after_multiple_next);
}
END_TCASE