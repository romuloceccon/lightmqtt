#include "check_lightmqtt.h"

#define ENTRY_COUNT 16

#define PREPARE \
    int data[ENTRY_COUNT]; \
    lmqtt_store_entry_t entries[ENTRY_COUNT]; \
    int class = -1; \
    lmqtt_store_value_t value_in; \
    lmqtt_store_value_t value_out; \
    int res; \
    lmqtt_store_t store; \
    size_t count = (size_t) -1; \
    long secs = -1, nsecs = -1; \
    do { \
        memset(data, 0xcc, sizeof(data)); \
        memset(entries, 0, sizeof(entries)); \
        memset(&store, 0, sizeof(store)); \
        memset(&value_out, 0xcc, sizeof(value_out)); \
        store.get_time = &test_time_get; \
        store.entries = entries; \
        store.capacity = ENTRY_COUNT; \
        value_in.value = NULL; \
        value_in.callback = &callback; \
        value_in.callback_data = &callback_data; \
    } while(0)

static int callback_data = 0;

int callback(void *callback_data, void *value)
{
    return 0;
}

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

    value_in.value = &data[0];
    res = lmqtt_store_append(&store, LMQTT_CLASS_CONNECT, &value_in);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_CONNECT, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
}
END_TEST

START_TEST(should_append_multiple_objects)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    ck_assert_int_eq(1, res);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
}
END_TEST

START_TEST(should_append_null_object)
{
    PREPARE;

    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, NULL);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(NULL, value_out.value);
}
END_TEST

START_TEST(should_not_append_object_if_store_is_full)
{
    int i, data_n;

    PREPARE;

    for (i = 0; i < store.capacity; i++) {
        value_in.packet_id = i;
        lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    }

    value_in.packet_id = i;
    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_pop_object)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 1,
        &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], value_out.value);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_not_pop_nonexistent_object)
{
    PREPARE;

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 0,
        &value_out);
    ck_assert_int_eq(0, res);
    ck_assert_ptr_eq(NULL, value_out.value);
    ck_assert(!value_out.callback);
    ck_assert_ptr_eq(NULL, value_out.callback_data);
}
END_TEST

START_TEST(should_get_second_object_after_popping_first_one)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 1,
        &value_out);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
}
END_TEST

START_TEST(should_get_first_object_after_popping_second_one)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 2,
        &value_out);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
}
END_TEST

START_TEST(should_shift_object)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_shift_object_with_null_pointers)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_shift(&store, NULL, NULL);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_shift(&store, NULL, NULL);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_not_peek_nonexistent_object)
{
    PREPARE;

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, class);
    ck_assert_ptr_eq(NULL, value_out.value);
}
END_TEST

START_TEST(should_peek_object)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
}
END_TEST

START_TEST(should_not_peek_first_object_after_mark)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    res = lmqtt_store_mark_current(&store);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_peek_second_object_after_mark)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    res = lmqtt_store_mark_current(&store);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
}
END_TEST

START_TEST(should_append_and_peek_after_mark)
{
    PREPARE;

    res = lmqtt_store_mark_current(&store);
    ck_assert_int_eq(0, res);

    value_in.packet_id = 1;
    value_in.value = &data[0];
    res = lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[0], value_out.value);
}
END_TEST

START_TEST(should_not_pop_before_mark)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 1,
        &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_peek_object_after_pop)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_pop_marked_by(&store, LMQTT_CLASS_PUBLISH_1, 1,
        &value_out);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
}
END_TEST

START_TEST(should_peek_object_after_shift)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(LMQTT_CLASS_PUBLISH_1, class);
    ck_assert_ptr_eq(&data[1], value_out.value);
}
END_TEST

START_TEST(should_drop_current_object)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_mark_current(&store);
    ck_assert_int_eq(1, res);
    res = lmqtt_store_drop_current(&store);
    ck_assert_int_eq(1, res);

    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], value_out.value);
    res = lmqtt_store_shift(&store, &class, &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_not_drop_nonexistent_object)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_drop_current(&store);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_unmark_all)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    lmqtt_store_mark_current(&store);
    lmqtt_store_mark_current(&store);

    lmqtt_store_unmark_all(&store);

    res = lmqtt_store_peek(&store, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[0], value_out.value);
}
END_TEST

START_TEST(should_count_items)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    ck_assert_int_eq(2, lmqtt_store_count(&store));
}
END_TEST

START_TEST(should_get_item_at_position)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 3;
    value_in.value = &data[2];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_get_at(&store, 1, &class, &value_out);
    ck_assert_int_eq(1, res);
    ck_assert_ptr_eq(&data[1], value_out.value);
    ck_assert_int_eq(3, lmqtt_store_count(&store));
}
END_TEST

START_TEST(should_not_get_nonexistent_item)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_get_at(&store, 1, &class, &value_out);
    ck_assert_int_eq(0, res);
}
END_TEST

START_TEST(should_delete_item_at_position)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 2;
    value_in.value = &data[1];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    value_in.packet_id = 3;
    value_in.value = &data[2];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);
    lmqtt_store_mark_current(&store);

    res = lmqtt_store_delete_at(&store, 1);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(2, lmqtt_store_count(&store));
}
END_TEST

START_TEST(should_not_delete_nonexistent_item)
{
    PREPARE;

    value_in.packet_id = 1;
    value_in.value = &data[0];
    lmqtt_store_append(&store, LMQTT_CLASS_PUBLISH_1, &value_in);

    res = lmqtt_store_delete_at(&store, 1);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(1, lmqtt_store_count(&store));
}
END_TEST

START_TEST(should_get_timeout_before_touch)
{
    PREPARE;

    store.timeout = 10;
    store.keep_alive = 15;

    test_time_set(5, 0);
    res = lmqtt_store_get_timeout(&store, &count, &secs, &nsecs);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, count);
    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_timeout_after_touch)
{
    PREPARE;

    store.timeout = 10;
    store.keep_alive = 15;

    test_time_set(4, 0);
    lmqtt_store_touch(&store);

    test_time_set(10, 0);
    res = lmqtt_store_get_timeout(&store, &count, &secs, &nsecs);
    ck_assert_int_eq(1, res);
    ck_assert_int_eq(0, count);
    ck_assert_int_eq(9, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TEST(should_get_timeout_after_touch_with_zeroed_keep_alive)
{
    PREPARE;

    store.timeout = 10;
    store.keep_alive = 0;

    test_time_set(4, 0);
    lmqtt_store_touch(&store);

    test_time_set(10, 0);
    res = lmqtt_store_get_timeout(&store, &count, &secs, &nsecs);
    ck_assert_int_eq(0, res);
    ck_assert_int_eq(0, count);
    ck_assert_int_eq(0, secs);
    ck_assert_int_eq(0, nsecs);
}
END_TEST

START_TCASE("Store")
{
    ADD_TEST(should_get_id);
    ADD_TEST(should_append_one_object);
    ADD_TEST(should_append_multiple_objects);
    ADD_TEST(should_append_null_object);
    ADD_TEST(should_not_append_object_if_store_is_full);
    ADD_TEST(should_pop_object);
    ADD_TEST(should_not_pop_nonexistent_object);
    ADD_TEST(should_get_second_object_after_popping_first_one);
    ADD_TEST(should_get_first_object_after_popping_second_one);
    ADD_TEST(should_shift_object);
    ADD_TEST(should_shift_object_with_null_pointers);
    ADD_TEST(should_not_peek_nonexistent_object);
    ADD_TEST(should_peek_object);
    ADD_TEST(should_not_peek_first_object_after_mark);
    ADD_TEST(should_peek_second_object_after_mark);
    ADD_TEST(should_append_and_peek_after_mark);
    ADD_TEST(should_not_pop_before_mark);
    ADD_TEST(should_peek_object_after_pop);
    ADD_TEST(should_peek_object_after_shift);
    ADD_TEST(should_drop_current_object);
    ADD_TEST(should_not_drop_nonexistent_object);
    ADD_TEST(should_unmark_all);
    ADD_TEST(should_count_items);
    ADD_TEST(should_get_item_at_position);
    ADD_TEST(should_not_get_nonexistent_item);
    ADD_TEST(should_delete_item_at_position);
    ADD_TEST(should_not_delete_nonexistent_item);
    ADD_TEST(should_get_timeout_before_touch);
    ADD_TEST(should_get_timeout_after_touch);
    ADD_TEST(should_get_timeout_after_touch_with_zeroed_keep_alive);
}
END_TCASE
