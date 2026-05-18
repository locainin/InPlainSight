// InPlainSight C test module
// Keep checks small, explicit, and tied to one IO behavior

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/error.h"
#include "../include/io.h"

static int check_true(int condition_value, const char *message) {
    if (!condition_value) {
        (void)fputs(message, stderr);
        (void)fputc('\n', stderr);
        return 0;
    }
    return 1;
}

static int test_expand_home_path_keeps_short_display_input_usable(void) {
    char expanded_path[PLAINSIGHT_MAX_PATH_BYTES];
    const char *home_path = getenv("HOME");
    size_t home_len = 0u;

    if (home_path == NULL || home_path[0] == '\0') {
        return 1;
    }

    if (!check_true(plainsight_io_expand_home_path("~/Pictures/cover.png",
                         expanded_path,
                         sizeof(expanded_path)) == PLAINSIGHT_OK,
                    "~/ expansion should succeed")) {
        return 0;
    }

    while (home_path[home_len] != '\0') {
        home_len++;
    }

    return check_true(strncmp(expanded_path, home_path, home_len) == 0,
               "expanded path should start with HOME") &&
           check_true(strstr(expanded_path, "/Pictures/cover.png") != NULL,
               "expanded path should keep the suffix");
}

static int test_expand_home_path_keeps_other_tilde_literal(void) {
    char expanded_path[PLAINSIGHT_MAX_PATH_BYTES];

    if (!check_true(plainsight_io_expand_home_path("~other/file.png",
                         expanded_path,
                         sizeof(expanded_path)) == PLAINSIGHT_OK,
                    "~other path should stay literal")) {
        return 0;
    }

    return check_true(strcmp(expanded_path, "~other/file.png") == 0,
               "~other must not be guessed");
}

int main(void) {
    int ok = 1;
    ok = test_expand_home_path_keeps_short_display_input_usable() && ok;
    ok = test_expand_home_path_keeps_other_tilde_literal() && ok;
    return ok ? 0 : 1;
}
