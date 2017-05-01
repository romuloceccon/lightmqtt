#ifndef _TESTS_CHECK_LIGHTMQTT_H_
#define _TESTS_CHECK_LIGHTMQTT_H_

#include <stdlib.h>
#include <check.h>
#include <assert.h>

#define START_TCASE(suite_name) \
    void tcase_add_methods(TCase *tcase); \
    \
    Suite* lightmqtt_suite(void) \
    { \
        Suite *result = suite_create(suite_name); \
        TCase *tcase = tcase_create("tcase"); \
        tcase_add_methods(tcase); \
        suite_add_tcase(result, tcase); \
        return result; \
    } \
    \
    void tcase_add_methods(TCase *tcase)

#define END_TCASE \
    int main(void) \
    { \
        int number_failed; \
        Suite *s; \
        SRunner *sr; \
        \
        s = lightmqtt_suite(); \
        sr = srunner_create(s); \
        \
        srunner_run_all(sr, CK_NORMAL); \
        number_failed = srunner_ntests_failed(sr); \
        srunner_free(sr); \
        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE; \
    }

#define ADD_TEST(method) tcase_add_test(tcase, method)

#include "lightmqtt/packet.h"
#include "lightmqtt/io.h"
#include "lightmqtt/types.h"

typedef struct {
    u8 buf[8192];
    int pos;
    int len;
    int available_len;
    int call_count;
} test_buffer_t;

typedef struct {
    test_buffer_t read_buf;
    test_buffer_t write_buf;
    int test_pos_read;
    int test_pos_write;
} test_socket_t;

typedef enum {
    TEST_CONNECT = 5000,
    TEST_SUBSCRIBE,
    TEST_UNSUBSCRIBE,
    TEST_PUBLISH,
    TEST_PUBREL,
    TEST_PINGREQ,
    TEST_DISCONNECT
} test_type_request_t;

typedef enum {
    TEST_CONNACK_SUCCESS = 5100,
    TEST_CONNACK_FAILURE,
    TEST_SUBACK_SUCCESS,
    TEST_UNSUBACK_SUCCESS,
    TEST_PUBLISH_QOS_2,
    TEST_PUBLISH_QOS_0_BIG,
    TEST_PUBACK,
    TEST_PUBREC,
    TEST_PUBCOMP,
    TEST_PINGRESP
} test_type_response_t;

lmqtt_io_result_t test_buffer_move(test_buffer_t *test_buffer, u8 *dst, u8 *src,
    int len, int *bytes_written);
lmqtt_io_result_t test_buffer_read(void *data, u8 *buf, int buf_len,
    int *bytes_read);
lmqtt_io_result_t test_buffer_write(void *data, u8 *buf, int buf_len,
    int *bytes_written);

lmqtt_io_result_t test_time_get(long *secs, long *nsecs);
void test_time_set(long secs, long nsecs);

lmqtt_io_result_t test_socket_read(void *data, u8 *buf, int buf_len,
    int *bytes_read);
lmqtt_io_result_t test_socket_write(void *data, u8 *buf, int buf_len,
    int *bytes_written);

void test_socket_init(test_socket_t *socket);
void test_socket_append_param(test_socket_t *socket, int val, int param);
void test_socket_append(test_socket_t *socket, int val);
int test_socket_shift(test_socket_t *socket);

/*
 * private functions which will be tested
 */

lmqtt_encode_result_t encode_remaining_length(int len, u8 *buf,
    int *bytes_written);

lmqtt_encode_result_t encode_buffer_encode(
    lmqtt_encode_buffer_t *encode_buffer, lmqtt_store_value_t *value,
    encode_buffer_builder_t builder, int offset, u8 *buf, int buf_len,
    int *bytes_written);

lmqtt_encode_result_t string_encode(lmqtt_string_t *str, int encode_len,
    int encode_if_empty, int offset, u8 *buf, int buf_len, int *bytes_written,
    lmqtt_string_t **blocking_str);

lmqtt_decode_result_t string_put(lmqtt_string_t *str, u8 b,
    lmqtt_string_t **blocking_str);

lmqtt_decode_result_t fixed_header_decode(lmqtt_fixed_header_t *header, u8 b);

lmqtt_encode_result_t connect_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer);

lmqtt_encode_result_t connect_encode_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t connect_build_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer);

lmqtt_encode_result_t connect_encode_variable_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t connect_encode_payload_client_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t connect_encode_payload_user_name(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t publish_build_fixed_header(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer);

lmqtt_encode_result_t publish_encode_topic(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t publish_encode_packet_id(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_encode_result_t publish_encode_payload(
    lmqtt_store_value_t *value, lmqtt_encode_buffer_t *encode_buffer,
    int offset, u8 *buf, int buf_len, int *bytes_written);

lmqtt_decode_result_t rx_buffer_decode_connack(
    lmqtt_rx_buffer_t *state, u8 b);

lmqtt_decode_result_t rx_buffer_decode_publish(
    lmqtt_rx_buffer_t *state, u8 b);

lmqtt_decode_result_t rx_buffer_decode_suback(
    lmqtt_rx_buffer_t *state, u8 b);

int rx_buffer_pubrel(lmqtt_rx_buffer_t *state);

extern int (*rx_buffer_call_callback)(lmqtt_rx_buffer_t *);
extern lmqtt_decode_result_t (*rx_buffer_decode_type)(lmqtt_rx_buffer_t *, u8);
extern lmqtt_encoder_finder_t (*tx_buffer_finder_by_class)(lmqtt_class_t);

lmqtt_io_status_t client_process_input(lmqtt_client_t *client);
lmqtt_io_status_t client_process_output(lmqtt_client_t *client);
lmqtt_io_status_t client_keep_alive(lmqtt_client_t *client);

#endif
