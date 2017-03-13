#include "check_lightmqtt.h"

#include "../src/lmqtt_store.c"

#define PREPARE \
    int data[LMQTT_STORE_SIZE]; \
    void *data_addr; \
    u16 packet_id = 0xffff; \
    lmqtt_class_t class = -1; \
    int res; \
    lmqtt_store_t store; \
    do { \
        memset(&data, 0xcc, sizeof(data)); \
        memset(&store, 0, sizeof(store)); \
    } while(0)

START_TEST(should_get_id)
{
    PREPARE;

    ck_assert_uint_eq(0, lmqtt_store_get_id(&store));
    ck_assert_uint_eq(1, lmqtt_store_get_id(&store));
    ck_assert_uint_eq(2, lmqtt_store_get_id(&store));
}
END_TEST

START_TEST(should_save_connect_object)
{
    PREPARE;

    res = lmqtt_store_save(&store, LMQTT_CLASS_CONNECT, &data[0], &packet_id);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(0, packet_id);

    res = lmqtt_store_peek(&store, LMQTT_CLASS_CONNECT, 0, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_not_save_connect_object_twice)
{
    PREPARE;

    res = lmqtt_store_save(&store, LMQTT_CLASS_CONNECT, &data[0], &packet_id);
    res = lmqtt_store_save(&store, LMQTT_CLASS_CONNECT, &data[1], &packet_id);
    ck_assert_int_eq(0, res);

    res = lmqtt_store_peek(&store, LMQTT_CLASS_CONNECT, 0, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_ignore_connect_packet_id)
{
    PREPARE;

    res = lmqtt_store_save(&store, LMQTT_CLASS_CONNECT, &data[0], &packet_id);
    res = lmqtt_store_peek(&store, LMQTT_CLASS_CONNECT, 123, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_save_publish_objects)
{
    PREPARE;

    res = lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[0], &packet_id);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(0, packet_id);
    res = lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[1], &packet_id);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(1, packet_id);

    res = lmqtt_store_peek(&store, LMQTT_CLASS_PUBLISH, 0, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
    res = lmqtt_store_peek(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_not_return_nonexistent_object)
{
    PREPARE;

    res = lmqtt_store_peek(&store, LMQTT_CLASS_CONNECT, 0, &data_addr);
    ck_assert_int_eq(0, res);
    ck_assert_ptr_eq(NULL, data_addr);
}
END_TEST

START_TEST(should_not_save_object_if_store_is_full)
{
    int i, data_n;

    PREPARE;

    for (i = 0; i < LMQTT_STORE_SIZE; i++)
        lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[i], &packet_id);

    res = lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data_n, &packet_id);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, packet_id);
}
END_TEST

START_TEST(should_pop_object)
{
    PREPARE;

    res = lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[0], &packet_id);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(0, packet_id);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 0, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
    res = lmqtt_store_peek(&store, LMQTT_CLASS_PUBLISH, 0, &data_addr);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_get_second_object_after_popping_first_one)
{
    PREPARE;

    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[0], &packet_id);
    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[1], &packet_id);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 0, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[1], data_addr);
}
END_TEST

START_TEST(should_get_first_object_after_popping_second_one)
{
    PREPARE;

    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[0], &packet_id);
    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[1], &packet_id);

    res = lmqtt_store_pop(&store, LMQTT_CLASS_PUBLISH, 1, &data_addr);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, LMQTT_CLASS_PUBLISH, 0, &data_addr);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], data_addr);
}
END_TEST

START_TEST(should_pop_any_object)
{
    PREPARE;

    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[0], &packet_id);
    lmqtt_store_save(&store, LMQTT_CLASS_PUBLISH, &data[1], &packet_id);

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

START_TCASE("Store")
{
    ADD_TEST(should_get_id);
    ADD_TEST(should_save_connect_object);
    ADD_TEST(should_not_save_connect_object_twice);
    ADD_TEST(should_ignore_connect_packet_id);
    ADD_TEST(should_save_publish_objects);
    ADD_TEST(should_not_return_nonexistent_object);
    ADD_TEST(should_not_save_object_if_store_is_full);
    ADD_TEST(should_pop_object);
    ADD_TEST(should_get_second_object_after_popping_first_one);
    ADD_TEST(should_get_first_object_after_popping_second_one);
    ADD_TEST(should_pop_any_object);
}
END_TCASE
