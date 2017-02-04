#ifndef _TESTS_CHECK_LIGHTMQTT_H_
#define _TESTS_CHECK_LIGHTMQTT_H_

TCase *tcase_encode_remaining_length(void);
TCase *tcase_validate_connect(void);
TCase *tcase_encode_connect_headers(void);
TCase *tcase_encode_connect_payload(void);
TCase *tcase_build_tx_buffer(void);
TCase *tcase_decode_connack(void);
TCase *tcase_decode_fixed_header(void);
TCase *tcase_process_rx_buffer(void);
TCase *tcase_process_input(void);

#endif
