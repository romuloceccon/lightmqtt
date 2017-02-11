#ifndef _TESTS_CHECK_LIGHTMQTT_H_
#define _TESTS_CHECK_LIGHTMQTT_H_

#include <stdlib.h>
#include <check.h>

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

#endif
