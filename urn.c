#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jansson.h>
#include <libgen.h>
#include "urn.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

long long urn_time_now(void) {
    struct timespec timespec;
    clock_gettime(CLOCK_MONOTONIC, &timespec);
    return timespec.tv_sec * 1000000L + timespec.tv_nsec / 1000;
}

long long urn_time_value(const char *string) {
    char seconds_part[256];
    double subseconds_part = 0.;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int sign = 1;
    if (!string || !strlen(string)) {
        return 0;
    }
    sscanf(string, "%[^.]%lf", seconds_part, &subseconds_part);
    string = seconds_part;
    if (string[0] == '-') {
        sign = -1;
        ++string;
    }
    switch (sscanf(string, "%d:%d:%d", &hours, &minutes, &seconds)) {
    case 2:
        seconds = minutes;
        minutes = hours;
        hours = 0;
        break;
    case 1:
        seconds = hours;
        minutes = 0;
        hours = 0;
        break;
    }
    return sign * ((hours * 60 * 60
                    + minutes * 60
                    + seconds) * 1000000L
                   + (int)(subseconds_part * 1000000.));
}

static void urn_time_string_format(char *string,
                                   char *millis,
                                   long long time,
                                   int serialized,
                                   int delta,
                                   int compact) {
    int hours, minutes, seconds;
    char dot_subsecs[256];
    const char *sign = "";
    if (time < 0) {
        time = -time;
        sign = "-";
    } else if (delta) {
        sign = "+";
    }
    hours = time / (1000000LL * 60 * 60);
    minutes = (time / (1000000LL * 60)) % 60;
    seconds = (time / 1000000LL) % 60;
    sprintf(dot_subsecs, ".%06d", time % 1000000LL);
    if (!serialized) {
        /* Show only a dot and 2 decimal places instead of all 6 */
        dot_subsecs[3] = '\0';
    }
    if (millis) {
        strcpy(millis, &dot_subsecs[1]);
        dot_subsecs[0] = '\0';
    }
    if (hours) {
        if (!compact) {
            sprintf(string, "%s%d:%02d:%02d%s",
                    sign, hours, minutes, seconds, dot_subsecs);
        } else {
            sprintf(string, "%s%d:%02d:%02d", sign, hours, minutes,seconds);
        }
    } else if (minutes) {
        if (!compact) {
            sprintf(string, "%s%d:%02d%s",
                    sign, minutes, seconds, dot_subsecs);
        } else {
            sprintf(string, "%s%d:%02d", sign, minutes, seconds);
        }
    } else {
        sprintf(string, "%s%d%s", sign, seconds, dot_subsecs);
    }
}

static void urn_time_string_serialized(char *string,
                                       long long time) {
    urn_time_string_format(string, NULL, time, 1, 0, 0);
}

void urn_time_string(char *string, long long time) {
    urn_time_string_format(string, NULL, time, 0, 0, 0);
}

void urn_time_millis_string(char *seconds, char *millis, long long time) {
    urn_time_string_format(seconds, millis, time, 0, 0, 0);
}

void urn_split_string(char *string, long long time) {
    urn_time_string_format(string, NULL, time, 0, 0, 1);
}

void urn_delta_string(char *string, long long time) {
    urn_time_string_format(string, NULL, time, 0, 1, 1);
}

void numtoord(char* string, int num) {
    if (11 <= num && num <= 13) {
        snprintf(string, sizeof(string), "%dth", num);
    }
    else if (num % 10 == 1) {
        snprintf(string, sizeof(string), "%dst", num);
    }
    else if (num % 10 == 2) {
        snprintf(string, sizeof(string), "%dnd", num);
    }
    else if (num % 10 == 3) {
        snprintf(string, sizeof(string), "%drd", num);
    }
    else {
        snprintf(string, sizeof(string), "%dth", num);
    }
}

void urn_game_release(urn_game *game) {
    int i;
    if (game->path) {
        free(game->path);
    }
    if (game->title) {
        free(game->title);
    }
    if (game->theme) {
        free(game->theme);
    }
    if (game->theme_variant) {
        free(game->theme_variant);
    }
    if (game->split_titles) {
        for (i = 0; i < game->split_count; ++i) {
            if (game->split_titles[i]) {
                free(game->split_titles[i]);
            }
        }
        free(game->split_titles);
    }
    if (game->split_times) {
        free(game->split_times);
    }
    if (game->segment_times) {
        free(game->segment_times);
    }
    if (game->best_splits) {
        free(game->best_splits);
    }
    if (game->best_segments) {
        free(game->best_segments);
    }
    if (game->datapath) {
        free(game->datapath);
    }
    if (game->segment_data) {
        for (i = 0; i < game->split_count; ++i) {
            if (game->segment_data[i]) {
                free(game->segment_data[i]);
            }
        }
        free(game->segment_data);
    }
    if (game->split_data) {
        for (i = 0; i < game->split_count; ++i) {
            if (game->split_data[i]) {
                free(game->split_data[i]);
            }
        }
        free(game->split_data);
    }
}
int cmp (const void *a, const void *b) {
    return ( *(long long*)a - *(long long*)b);
}
void urn_game_load_data(urn_game *game) {
    int c, i;
    // populate data
    FILE *datafp = fopen(game->datapath, "r");
    if (datafp) {
        while ((c = getc(datafp)) != EOF) {
            if (c == '\n') {
                game->data_size++;
            }
        }
        game->split_data = calloc(game->split_count, sizeof(long long*));
        game->segment_data = calloc(game->split_count, sizeof(long long*));
        for (int i=0; i<game->split_count; ++i) {
            if(game->split_data[i]) {
                free(game->split_data[i]);
            }
            if(game->segment_data[i]) {
                free(game->segment_data[i]);
            }
            game->split_data[i] = calloc(game->data_size, sizeof(long long));
            game->segment_data[i] = calloc(game->data_size, sizeof(long long));
        }
        rewind(datafp);
        int col = -1; //first is date
        int row = 0;
        char buffer[(game->split_count+2) * sizeof(long long)];
        while (fgets(buffer, sizeof(buffer), datafp) != NULL ) {
            char *token = strtok(buffer, ",");
            while (token) {
                if (col >= 0 && *token != '\n') {
                    game->split_data[col][row] = atoll(token);
                    if (col == 0) {
                        game->segment_data[col][row] = game->split_data[col][row];
                    } else if (game->split_data[col][row] == 0 || game->split_data[col-1][row] == 0) {
                        game->segment_data[col][row] = 0;
                    }
                    else {
                        game->segment_data[col][row] = game->split_data[col][row] - game->split_data[col-1][row];
                    }
                }
                col++;
                token = strtok(NULL, ",");
            }
            col = -1;
            row++;
        }
        fclose(datafp);
    }

    for (i=0; i<game->split_count; ++i) {
        qsort(game->split_data[i], game->data_size, sizeof(long long), cmp);
        qsort(game->segment_data[i], game->data_size, sizeof(long long), cmp);
    }
}

int urn_game_create(urn_game **game_ptr, const char *path) {
    int error = 0;
    urn_game *game;
    int i;
    json_t *json = 0;
    json_t *ref;
    json_error_t json_error;
    // allocate game
    game = calloc(1, sizeof(urn_game));
    if (!game) {
        error = 1;
        goto game_create_done;
    }
    // copy path to file
    game->data_size = 0;
    game->path = NULL;
    game->path = strdup(path);
    if (!game->path) {
        error = 1;
        goto game_create_done;
    }
    // generate stat file path
    game->datapath = strdup(path);
    char *extn = NULL;
    extn = strstr(game->datapath, ".json");
    if (extn != NULL) {
        *extn = '\0';
    }
    strcat(game->datapath, ".stat");
    if (!game->datapath) {
        error = 1;
        goto game_create_done;
    }
    // load json
    json = json_load_file(game->path, 0, &json_error);
    if (!json) {
        error = 1;
        goto game_create_done;
    }
    // copy title
    ref = json_object_get(json, "title");
    if (ref) {
        game->title = strdup(json_string_value(ref));
        if (!game->title) {
            error = 1;
            goto game_create_done;
        }
    }
    // copy theme
    ref = json_object_get(json, "theme");
    if (ref) {
        game->theme = strdup(json_string_value(ref));
        if (!game->theme) {
            error = 1;
            goto game_create_done;
        }
    }
    // copy theme variant
    ref = json_object_get(json, "theme_variant");
    if (ref) {
        game->theme_variant = strdup(json_string_value(ref));
        if (!game->theme_variant) {
            error = 1;
            goto game_create_done;
        }
    }
    // get attempt count
    ref = json_object_get(json, "attempt_count");
    if (ref) {
        game->attempt_count = json_integer_value(ref);
    }
    // get width
    ref = json_object_get(json, "width");
    if (ref) {
        game->width = json_integer_value(ref);
    }
    // get height
    ref = json_object_get(json, "height");
    if (ref) {
        game->height = json_integer_value(ref);
    }
    // get delay
    ref = json_object_get(json, "start_delay");
    if (ref) {
        game->start_delay = urn_time_value(
            json_string_value(ref));
    }
    // get wr
    ref = json_object_get(json, "world_record");
    if (ref) {
        game->world_record = urn_time_value(
            json_string_value(ref));
    }
    // get splits
    ref = json_object_get(json, "splits");
    if (ref) {
        game->split_count = json_array_size(ref);
        // allocate titles
        game->split_titles = calloc(game->split_count,
                                    sizeof(char *));
        if (!game->split_titles) {
            error = 1;
            goto game_create_done;
        }
        // allocate splits
        game->split_times = calloc(game->split_count,
                                   sizeof(long long));
        if (!game->split_times) {
            error = 1;
            goto game_create_done;
        }
        game->segment_times = calloc(game->split_count,
                                     sizeof(long long));
        if (!game->segment_times) {
            error = 1;
            goto game_create_done;
        }
        game->best_splits = calloc(game->split_count,
                                   sizeof(long long));
        if (!game->best_splits) {
            error = 1;
            goto game_create_done;
        }
        game->best_segments = calloc(game->split_count,
                                     sizeof(long long));
        if (!game->best_segments) {
            error = 1;
            goto game_create_done;
        }
        // copy splits
        for (i = 0; i < game->split_count; ++i) {
            json_t *split;
            json_t *split_ref;
            split = json_array_get(ref, i);
            split_ref = json_object_get(split, "title");
            if (split_ref) {
                game->split_titles[i] = strdup(
                    json_string_value(split_ref));
                if (!game->split_titles[i]) {
                    error = 1;
                    goto game_create_done;
                }
            }
            split_ref = json_object_get(split, "time");
            if (split_ref) {
                game->split_times[i] = urn_time_value(
                    json_string_value(split_ref));
            }
            if (i && game->split_times[i] && game->split_times[i-1]) {
                game->segment_times[i] = game->split_times[i] - game->split_times[i-1];
            } else if (!i && game->split_times[0]) {
                game->segment_times[0] = game->split_times[0];
            }
            split_ref = json_object_get(split, "best_time");
            if (split_ref) {
                game->best_splits[i] = urn_time_value(
                    json_string_value(split_ref));
            } else if (game->split_times[i]) {
                game->best_splits[i] = game->split_times[i];
            }
            split_ref = json_object_get(split, "best_segment");
            if (split_ref) {
                game->best_segments[i] = urn_time_value(
                    json_string_value(split_ref));
            } else if (game->segment_times[i]) {
                game->best_segments[i] = game->segment_times[i];
            }
        }
    }

    FILE *datafp = fopen(game->datapath, "r");
    if (datafp) {
        int c;
        while ((c = getc(datafp)) != EOF) {
            if (c == '\n') {
                game->data_size++;
            }
        }

        fclose(datafp);
        urn_game_load_data(game);
    }

    // populate data
   /* FILE *datafp = fopen(game->datapath, "r");
    if (datafp) {
        int c;
        while ((c = getc(datafp)) != EOF) {
            if (c == '\n') {
                game->data_size++;
            }
        }

        long long **spdata = calloc(game->split_count, sizeof(long long*));
        long long **segdata = calloc(game->split_count, sizeof(long long*));
        game->split_data = spdata;
        game->segment_data = segdata;

        for (i=0; i<game->split_count; ++i) {
            spdata[i] = calloc(game->data_size, sizeof(long long));
            segdata[i] = calloc(game->data_size, sizeof(long long));
            game->split_data[i] = spdata[i];
            game->segment_data[i] = segdata[i];
        }

        rewind(datafp);
        int col = -1;
        int row = 0;
        char buffer[(game->split_count+2) * sizeof(long long)];
        while (fgets(buffer, sizeof(buffer), datafp) != NULL ) {
            char *token = strtok(buffer, ",");
            while (token) {
                if (col >= 0 && *token != '\n') {
                    spdata[col][row] = atoll(token);
                    if (col == 0) {
                        segdata[col][row] = spdata[col][row];
                    } else if (spdata[col][row] == 0 || spdata[col-1][row] == 0) {
                        segdata[col][row] = 0;
                    }
                    else {
                        segdata[col][row] = spdata[col][row] - spdata[col-1][row];
                    }
                }
                col++;
                token = strtok(NULL, ",");
            }
            col = -1;
            row++;
        }

        int cmp (const void *a, const void *b) {
            return ( *(long long*)a - *(long long*)b);
        }
        for (i=0; i<game->split_count; ++i) {
            qsort(game->split_data[i], game->data_size, sizeof(long long), cmp);
            qsort(game->segment_data[i], game->data_size, sizeof(long long), cmp);
        }
    }*/

game_create_done:
    if (!error) {
        *game_ptr = game;
    } else if (game) {
        urn_game_release(game);
    }
    if (json) {
        json_decref(json);
    }
    return error;
}

void urn_game_update_splits(urn_game *game,
                            const urn_timer *timer) {
    if (timer->curr_split) {
        int size;
        if (timer->split_times[game->split_count - 1]
            && timer->split_times[game->split_count - 1]
            < game->world_record) {
            game->world_record = timer->split_times[game->split_count - 1];
        }
        size = timer->curr_split * sizeof(long long);
        memcpy(game->split_times, timer->split_times, size);
        memcpy(game->segment_times, timer->segment_times, size);
        memcpy(game->best_splits, timer->best_splits, size);
        memcpy(game->best_segments, timer->best_segments, size);
    }
}

void urn_game_update_bests(urn_game *game,
                           const urn_timer *timer) {
    if (timer->curr_split) {
        int size;
        size = timer->curr_split * sizeof(long long);
        memcpy(game->best_splits, timer->best_splits, size);
        memcpy(game->best_segments, timer->best_segments, size);
    }
}

int urn_game_save(const urn_game *game) {
    int error = 0;
    char str[256];
    json_t *json = json_object();
    json_t *splits = json_array();
    int i;
    if (game->title) {
        json_object_set_new(json, "title", json_string(game->title));
    }
    if (game->attempt_count) {
        json_object_set_new(json, "attempt_count",
                            json_integer(game->attempt_count));
    }
    if (game->world_record) {
        urn_time_string_serialized(str, game->world_record);
        json_object_set_new(json, "world_record", json_string(str));
    }
    if (game->start_delay) {
        urn_time_string_serialized(str, game->start_delay);
        json_object_set_new(json, "start_delay", json_string(str));
    }
    for (i = 0; i < game->split_count; ++i) {
        json_t *split = json_object();
        json_object_set_new(split, "title",
                            json_string(game->split_titles[i]));
        urn_time_string_serialized(str, game->split_times[i]);
        json_object_set_new(split, "time", json_string(str));
        urn_time_string_serialized(str, game->best_splits[i]);
        json_object_set_new(split, "best_time", json_string(str));
        urn_time_string_serialized(str, game->best_segments[i]);
        json_object_set_new(split, "best_segment", json_string(str));
        json_array_append_new(splits, split);
    }
    json_object_set_new(json, "splits", splits);
    if (game->theme) {
        json_object_set_new(json, "theme", json_string(game->theme));
    }
    if (game->theme_variant) {
        json_object_set_new(json, "theme_variant",
                            json_string(game->theme_variant));
    }
    if (game->width) {
        json_object_set_new(json, "width", json_integer(game->width));
    }
    if (game->height) {
        json_object_set_new(json, "height", json_integer(game->height));
    }
    if (!json_dump_file(json, game->path,
                        JSON_PRESERVE_ORDER | JSON_INDENT(2))) {
        error = 1;
    }
    json_decref(json);
    return error;
}

int urn_timer_store(const urn_timer *timer) {
    if (timer->curr_split == 0) {
        /* No splits to save */
        return 0;
    }
    int error = 0;
    FILE* fp = fopen(timer->game->datapath, "a");
    if (!fp) {
        error = 1;
    }
    else {
        time_t rawtime = time(NULL);
        struct tm *loctime = localtime(&rawtime);
        fprintf(fp, "%d-%02d-%02d", loctime->tm_year + 1900,
                loctime->tm_mon + 1, loctime->tm_mday);
        int i;
        for (i = 0; i < timer->game->split_count; ++i) {
            if (timer->split_times[i]) {
                // human readable
                /* char segtime_str[256];
                urn_time_string_serialized(segtime_str, timer->segment_times[i]);
                fprintf(fp, ",%s", segtime_str);*/
                fprintf(fp, ",%d", timer->split_times[i]);
            }
            else {
                fprintf(fp, ",-");
            }
        }
        fprintf(fp, "\n");
        fclose(fp);
    }
    return error;
}

void urn_timer_release(urn_timer *timer) {
    if (timer->split_times) {
        free(timer->split_times);
    }
    if (timer->split_deltas) {
        free(timer->split_deltas);
    }
    if (timer->split_ptiles) {
        free(timer->split_ptiles);
    }
    if (timer->segment_times) {
        free(timer->segment_times);
    }
    if (timer->segment_deltas) {
        free(timer->segment_deltas);
    }
    if (timer->segment_ptiles) {
        free(timer->segment_ptiles);
    }
    if (timer->split_info) {
        free(timer->split_info);
    }
    if (timer->best_splits) {
        free(timer->best_splits);
    }
    if (timer->best_segments) {
        free(timer->best_segments);
    }
}

static void reset_timer(urn_timer *timer) {
    urn_timer_store(timer);
    urn_game_load_data(timer->game);
    int i;
    int size;
    timer->started = 0;
    timer->start_time = 0;
    timer->curr_split = 0;
    timer->time = -timer->game->start_delay;
    size = timer->game->split_count * sizeof(long long);
    memcpy(timer->split_times, timer->game->split_times, size);
    memset(timer->split_deltas, 0, size);
    memcpy(timer->segment_times, timer->game->segment_times, size);
    memset(timer->segment_deltas, 0, size);
    memcpy(timer->best_splits, timer->game->best_splits, size);
    memcpy(timer->best_segments, timer->game->best_segments, size);
    size = timer->game->split_count * sizeof(int);
    memset(timer->split_info, 0, size);
    memset(timer->split_ptiles, 0, size);
    memset(timer->segment_ptiles, 0, size);
    timer->sum_of_bests = 0;
    for (i = 0; i < timer->game->split_count; ++i) {
        if (timer->best_segments[i]) {
            timer->sum_of_bests += timer->best_segments[i];
        } else if (timer->game->best_segments[i]) {
            timer->sum_of_bests += timer->game->best_segments[i];
        } else {
            timer->sum_of_bests = 0;
            break;
        }
    }
}

int urn_timer_create(urn_timer **timer_ptr, urn_game *game) {
    int error = 0;
    urn_timer *timer;
    // allocate timer
    timer = calloc(1, sizeof(urn_timer));
    if (!timer) {
        error = 1;
        goto timer_create_done;
    }
    timer->game = game;
    timer->attempt_count = &game->attempt_count;
    // alloc splits
    timer->split_times = calloc(timer->game->split_count,
                                sizeof(long long));
    if (!timer->split_times) {
        error = 1;
        goto timer_create_done;
    }
    timer->split_deltas = calloc(timer->game->split_count,
                                 sizeof(long long));
    if (!timer->split_deltas) {
        error = 1;
        goto timer_create_done;
    }
    timer->segment_times = calloc(timer->game->split_count,
                                  sizeof(long long));
    if (!timer->segment_times) {
        error = 1;
        goto timer_create_done;
    }
    timer->segment_deltas = calloc(timer->game->split_count,
                                   sizeof(long long));
    if (!timer->segment_deltas) {
        error = 1;
        goto timer_create_done;
    }
    timer->best_splits = calloc(timer->game->split_count,
                                sizeof(long long));
    if (!timer->best_splits) {
        error = 1;
        goto timer_create_done;
    }
    timer->best_segments = calloc(timer->game->split_count,
                                  sizeof(long long));
    if (!timer->best_segments) {
        error = 1;
        goto timer_create_done;
    }
   timer->split_info = calloc(timer->game->split_count,
                               sizeof(int));
    if (!timer->split_info) {
        error = 1;
        goto timer_create_done;
    }
    timer->segment_ptiles = calloc(timer->game->split_count,
                                sizeof(int));
    if (!timer->segment_ptiles) {
        error = 1;
        goto timer_create_done;
    }
    timer->split_ptiles = calloc(timer->game->split_count,
                                sizeof(int));
    if (!timer->split_ptiles) {
        error = 1;
        goto timer_create_done;
    }
    reset_timer(timer);
 timer_create_done:
    if (!error) {
        *timer_ptr = timer;
    } else if (timer) {
        urn_timer_release(timer);
    }
    return error;
}

void urn_timer_step(urn_timer *timer, long long now) {
    timer->now = now;
    if (timer->running) {
        timer->time = timer->now - timer->start_time;
        long long time = timer->now - timer->start_time;
        if (timer->curr_split < timer->game->split_count) {
            timer->split_times[timer->curr_split] = time;
            // calc delta
            if (timer->game->split_times[timer->curr_split]) {
                timer->split_deltas[timer->curr_split] =
                    timer->split_times[timer->curr_split]
                    - timer->game->split_times[timer->curr_split];
            }
            // check for behind time
            if (timer->split_deltas[timer->curr_split] > 0) {
                timer->split_info[timer->curr_split] |= URN_INFO_BEHIND_TIME;
            } else {
                timer->split_info[timer->curr_split] &= ~URN_INFO_BEHIND_TIME;
            }
            if (!timer->curr_split || timer->split_times[timer->curr_split - 1]) {
                // calc segment time and delta
                timer->segment_times[timer->curr_split] =
                    timer->split_times[timer->curr_split];
                if (timer->curr_split) {
                    timer->segment_times[timer->curr_split] -=
                        timer->split_times[timer->curr_split - 1];
                }
                if (timer->game->segment_times[timer->curr_split]) {
                    timer->segment_deltas[timer->curr_split] =
                        timer->segment_times[timer->curr_split]
                        - timer->game->segment_times[timer->curr_split];
                }
            }
            // check for losing time
            if (timer->curr_split) {
                if (timer->split_deltas[timer->curr_split]
                    > timer->split_deltas[timer->curr_split - 1]) {
                    timer->split_info[timer->curr_split]
                        |= URN_INFO_LOSING_TIME;
                } else {
                    timer->split_info[timer->curr_split]
                        &= ~URN_INFO_LOSING_TIME;
                }
            } else if (timer->split_deltas[timer->curr_split] > 0) {
                timer->split_info[timer->curr_split]
                    |= URN_INFO_LOSING_TIME;
            } else {
                timer->split_info[timer->curr_split]
                    &= ~URN_INFO_LOSING_TIME;
            }
        }
    }
}

int urn_timer_start(urn_timer *timer) {
    if (timer->curr_split < timer->game->split_count) {
        if (!timer->start_time) {
            timer->start_time = timer->now + timer->game->start_delay;
            ++*timer->attempt_count;
            timer->started = 1;
        }
        timer->running = 1;
    }
    return timer->running;
}

int urn_timer_split(urn_timer *timer) {
    if (timer->running && timer->time > 0) {
        if (timer->curr_split < timer->game->split_count) {
            int i;
            // check for best split and segment
            if (!timer->best_splits[timer->curr_split]
                || timer->split_times[timer->curr_split]
                < timer->best_splits[timer->curr_split]) {
                timer->best_splits[timer->curr_split] =
                    timer->split_times[timer->curr_split];
                timer->split_info[timer->curr_split]
                    |= URN_INFO_BEST_SPLIT;
            }
            if (!timer->best_segments[timer->curr_split]
                || timer->segment_times[timer->curr_split]
                < timer->best_segments[timer->curr_split]) {
               timer->best_segments[timer->curr_split] =
                    timer->segment_times[timer->curr_split];
                timer->split_info[timer->curr_split]
                    |= URN_INFO_BEST_SEGMENT;
            }
            // update sum of bests
            timer->sum_of_bests = 0;
            for (i = 0; i < timer->game->split_count; ++i) {
                if (timer->best_segments[i]) {
                    timer->sum_of_bests += timer->best_segments[i];
                } else if (timer->game->best_segments[i]) {
                    timer->sum_of_bests += timer->game->best_segments[i];
                } else {
                    timer->sum_of_bests = 0;
                    break;
                }
            }
            // calculate percentiles ! @TODO
            int sp_flag = 0, seg_flag = 0;
            int sp_p = timer->game->data_size, seg_p = timer->game->data_size;
            int sp_z = 0, seg_z = 0;
            float sp_ptile, seg_ptile;
            i = 0;
            while (i < timer->game->data_size && !sp_flag && !seg_flag) {
                //split idx
                if (!sp_flag && timer->game->split_data[timer->curr_split][i]
                    < timer->split_times[timer->curr_split]) {
                        sp_p--;
                        if (timer->game->split_data[timer->curr_split][i] == 0) {
                            sp_z++;
                        }
                } else {
                    sp_flag = 1;
                }
                //seg idx
                if (!seg_flag && timer->game->segment_data[timer->curr_split][i]
                    < timer->segment_times[timer->curr_split]) {
                        seg_p--;
                        if (timer->game->segment_data[timer->curr_split][i] == 0) {
                            seg_z++;
                        }
                } else {
                    seg_flag = 1;
                }
                i++;
            }
            sp_ptile = 100*(sp_p) / (timer->game->data_size+1-sp_z);
            seg_ptile = 100*(seg_p) / (timer->game->data_size+1-seg_z);
            timer->split_ptiles[timer->curr_split] = (int) sp_ptile;
            timer->segment_ptiles[timer->curr_split] = (int) seg_ptile;

            // stop timer if last split
            if (timer->curr_split + 1 == timer->game->split_count) {
                urn_timer_stop(timer);
            }
            return ++timer->curr_split;
        }
    }
    return 0;
}

int urn_timer_skip(urn_timer *timer) {
    if (timer->running && timer->time > 0) {
        if (timer->curr_split < timer->game->split_count) {
            timer->split_times[timer->curr_split] = 0;
            timer->split_deltas[timer->curr_split] = 0;
            timer->split_ptiles[timer->curr_split] = 0;
            timer->split_info[timer->curr_split] = 0;
            timer->segment_times[timer->curr_split] = 0;
            timer->segment_deltas[timer->curr_split] = 0;
            timer->segment_ptiles[timer->curr_split] = 0;
            return ++timer->curr_split;
        }
    }
    return 0;
}

int urn_timer_unsplit(urn_timer *timer) {
    if (timer->curr_split) {
        int i;
        int curr = --timer->curr_split;
        for (i = curr; i < timer->game->split_count; ++i) {
            timer->split_times[i] = timer->game->split_times[i];
            timer->split_deltas[i] = 0;
            timer->split_ptiles[i] = 0;
            timer->segment_times[i] = timer->game->segment_times[i];
            timer->segment_deltas[i] = 0;
            timer->segment_ptiles[i] = 0;
            timer->split_info[i] = 0;
            //undo best splits
            timer->best_splits[i] = timer->game->best_splits[i];
            //undo best segments
            timer->best_segments[i] = timer->game->best_segments[i];
            // update sum of bests
            timer->sum_of_bests = 0;
            for (i = 0; i < timer->game->split_count; ++i) {
                if (timer->best_segments[i]) {
                    timer->sum_of_bests += timer->best_segments[i];
                } else if (timer->game->best_segments[i]) {
                    timer->sum_of_bests += timer->game->best_segments[i];
                } else {
                    timer->sum_of_bests = 0;
                    break;
                }
            }
        }
        if (timer->curr_split + 1 == timer->game->split_count) {
            timer->running = 1;
        }
        return timer->curr_split;
    }
    return 0;
}

void urn_timer_stop(urn_timer *timer) {
    timer->running = 0;
}

int urn_timer_reset(urn_timer *timer) {
    if (!timer->running) {
        if (timer->started && timer->time <= 0) {
            return urn_timer_cancel(timer);
        }
        reset_timer(timer);
        return 1;
    }
    return 0;
}

int urn_timer_cancel(urn_timer *timer) {
    if (!timer->running) {
        if (timer->started) {
            if (*timer->attempt_count <= 0) {
                *timer->attempt_count = 0;
            } else {
                --*timer->attempt_count;
            }
        }
        reset_timer(timer);
        return 1;
    }
    return 0;
}
