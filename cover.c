#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

typedef int Errno;

#define return_defer(value) do { result = (value); goto defer; } while (0)
#define UNUSED(x) (void)(x)

#define DA_INIT_CAP 256

#define da_append(da, item)                                                          \
    do {                                                                             \
        if ((da)->count >= (da)->capacity) {                                         \
            (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;   \
            (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Buy more RAM lol");                       \
        }                                                                            \
                                                                                     \
        (da)->items[(da)->count++] = (item);                                         \
    } while (0)


Errno read_entire_file(const char *file_path, char **buf, size_t *buf_len);

typedef struct {
    size_t count;
    const char *data;
} String_View;

static String_View sv_chop_by_delim(String_View *sv, char delim)
{
    size_t i = 0;
    while (i < sv->count && sv->data[i] != delim) {
        i += 1;
    }

    String_View result = {
        .data = sv->data,
        .count = i,
    };

    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data  += i + 1;
    } else {
        sv->count -= i;
        sv->data  += i;
    }

    return result;
}

uint64_t sv_to_u64(String_View sv)
{
    uint64_t result = 0;

    for (size_t i = 0; i < sv.count && isdigit(sv.data[i]); ++i) {
        result = result * 10 + (uint64_t) sv.data[i] - '0';
    }

    return result;
}

String_View sv_trim_left(String_View sv)
{
    size_t i = 0;
    while (i < sv.count && isspace(sv.data[i])) {
        i += 1;
    }

    sv.data += i;
    sv.count -= i;
    return sv;
}

typedef struct {
    String_View text;
    bool visited;
} Line;

typedef struct {
    Line *items;
    size_t count;
    size_t capacity;
} Lines;

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: %s <COVER> <stemmer.sbl>\n", argv[0]);
        exit(1);
    }

    Lines lines = {0};

    // Snowball Source Code
    {
        const char *stemmer_path = argv[2];
        char *stemmer_buf;
        size_t stemmer_buf_len;
        Errno err = read_entire_file(stemmer_path, &stemmer_buf, &stemmer_buf_len);
        if (err != 0) {
            printf("ERROR: could not read %s: %s\n", stemmer_path, strerror(errno));
            exit(1);
        }

        String_View stemmer_sv = {
            .data = stemmer_buf,
            .count = stemmer_buf_len,
        };
        while (stemmer_sv.count > 0) {
            String_View line = sv_chop_by_delim(&stemmer_sv, '\n');
            da_append(&lines, ((Line){
                .text = line
            }));
        }
    }    

    // COVER file
    {
        const char *cover_path = argv[1];
        char *cover_buf;
        size_t cover_buf_len;
        Errno err = read_entire_file(cover_path, &cover_buf, &cover_buf_len);
        if (err != 0) {
            printf("ERROR: could not read %s: %s\n", cover_path, strerror(errno));
            exit(1);
        }

        String_View cover_sv = {
            .data = cover_buf, 
            .count = cover_buf_len,
        };
        while (cover_sv.count > 0) {
            uint64_t line = sv_to_u64(sv_trim_left(sv_chop_by_delim(&cover_sv, '\n')));
            lines.items[line - 1].visited = true;
        }
    }

    for (size_t i = 0; i < lines.count; ++i) {
        if (lines.items[i].visited) {
            fputs("\033[48;5;5m", stdout);
            //               ^ color number
        }
        fwrite(lines.items[i].text.data, lines.items[i].text.count, 1, stdout);
        fputs("\033[0m\n", stdout);
    }

    return 0;
}

static Errno file_size(FILE *file, size_t *size)
{
    long saved = ftell(file);
    if (saved < 0) return errno;
    if (fseek(file, 0, SEEK_END) < 0) return errno;
    long result = ftell(file);
    if (result < 0) return errno;
    if (fseek(file, saved, SEEK_SET) < 0) return errno;
    *size = (size_t) result;
    return 0;
}

Errno read_entire_file(const char *file_path, char **buf, size_t *buf_len)
{
    Errno result = 0;
    FILE *f = NULL;

    f = fopen(file_path, "r");
    if (f == NULL) return_defer(errno);

    Errno err = file_size(f, buf_len);
    if (err != 0) return_defer(err);

    *buf = malloc(*buf_len);
    assert(*buf != NULL && "Buy more RAM lol");

    fread(*buf, *buf_len, 1, f);
    if (ferror(f)) return_defer(errno);

defer:
    if (f) fclose(f);
    return result;
}
