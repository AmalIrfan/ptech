#ifndef _PTECH_H_
#define _PTECH_H_

enum pt_return {
    FAILED = -1,
    SKIPPED = 0,
    SUCCESS = 1
};

enum pt_return
build(const char* input, const char* output, int force);

enum pt_return
preprocess(const char* input, const char* output, int force);

#ifdef PTECH_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* pt_string decs */

struct pt_string {
	int size;
	int capacity;
	char *data;
};

struct pt_string pt_string_create(void* buf, int capacity);

void pt_string_append_impl(struct pt_string* s, const char* delim, ...);
#define pt_string_append(...) pt_string_append_impl(__VA_ARGS__, 0)

int pt_string_exec(struct pt_string* s);


/* pt_string defs */

struct pt_string pt_string_create(void* buf, int capacity) {
	struct pt_string s = {0, capacity, buf};
	if (!buf)
		/* TODO: allocate */;
        /* null terminator */
        s.data[0] = 0;
	return s;
}

void pt_string_append_one(struct pt_string* s, const char* cs) {
	char ch = *cs;
	while (ch) {
		s->data[s->size] = ch;
		cs++;
		s->size++;
		ch = *cs;
                /* have room for null terminator? */
		if (s->size >= s->capacity - 1 && ch)
			/* TODO: inform */
			break;
	}
        s->data[s->size] = 0;
}


void pt_string_append_impl(struct pt_string* s, const char* delim, ...) {
    va_list args;
    const char *cs = 0;
    va_start(args, delim);
    while (1) {
        cs = va_arg(args, typeof(cs));
        if (!cs)
            break;
        pt_string_append_one(s, cs);
        if (delim && delim[0])
            pt_string_append_one(s, delim);
    }
    va_end(args);
}

int pt_string_exec(struct pt_string* s) {
    /* TODO: make safer */

    return system(s->data);
}

/* build */

int pt_get_mtime(const char* file);
int pt_need_build(const char* input, const char* output);

int build(const char* input, const char* output, int force) {
    printf("CC %s <- %s ", output, input);
    fflush(stdout);
    if (!pt_need_build(input, output) && !force) {
        puts(" skip.");
        return SKIPPED;
    }
    static char cmdbuf[1024];
    struct pt_string cmdline = pt_string_create(cmdbuf, sizeof(cmdbuf));

    pt_string_append(&cmdline, " ",
        "cc",
        "-g",
        "-o", output,
        input
    );

    int ret = pt_string_exec(&cmdline);

    if (ret) {
        puts(" no.");
        return FAILED;
    } else {
        puts(" ok.");
        return SUCCESS;
    }
}

int pt_get_mtime(const char* file) {
    struct stat result;
    if(stat(file, &result) == 0)
        return result.st_mtime;
    return 0;
}

int pt_need_build(const char* input, const char* output) {
    return pt_get_mtime(input) > pt_get_mtime(output);
}

/* preprocess */


#define PT_BLOCKBODY 1024
#define PT_LISTITEM (PT_BLOCKBODY/16)

struct pt_block {
    char name[128];
    enum {PT_ITEM, PT_LIST, PT_FOREACH} type;
    char body[PT_BLOCKBODY];
};

struct pt_block pt_blocks[10];
int pt_bx = 0;

struct pt_context {
    struct pt_block* bp;
    int i;
    int repeat;
    int capture;
    int escape;
};

struct pt_context pt_context[10];
int pt_cx = 0;

int pt_context_push(int bi, int capture, int repeat, int escape) {
    pt_context[pt_cx].bp = pt_blocks + bi;
    pt_context[pt_cx].i = repeat * PT_LISTITEM;
    pt_context[pt_cx].repeat = repeat;
    pt_context[pt_cx].capture = capture;
    pt_context[pt_cx].escape = escape;
    pt_cx++;
}

struct pt_context *pt_context_view() {
    return pt_context + pt_cx - 1;
}

void pt_context_pop() {
    struct pt_context* c = pt_context_view();
    c->bp->body[c->i++] = 0;
    c->bp->body[c->i++] = -1;
    pt_cx--;
}


int pt_getc(FILE* fh) {
    if (pt_cx) {
        struct pt_context* c = pt_context_view();
        if (!c->capture) {
            char ch = c->bp->body[c->i++];
            if (ch > 0)
                return ch;
            if (ch == 0)
                /* TODO: is this ok? */
                return ch;
            /* -1 is end to make pt_ungetc word */
            if (ch < 0) {
                if (c->bp->type == PT_FOREACH) {
                    c->repeat += 1;
                    /* check if there is c->repeat element */
                    const char* list = c->bp->name + 64;
                    for (int bi = 0; bi < pt_bx; bi++) {
                        if (strcmp(pt_blocks[bi].name, list) == 0) {
                            /* check if first character is not null */
                            if (pt_blocks[bi].body[c->repeat * PT_LISTITEM]) {
                                /* start over with new c->repeat */
                                c->i = 0;
                                return pt_getc(fh);
                            }
                        }
                    }
                }
                /* start over with next pt_context */
                pt_context_pop();
                return pt_getc(fh);
            }
        }
    }
    return getc(fh);
}

int pt_ungetc(char ch, FILE* fh) {
    if (pt_cx) {
        struct pt_context* c = pt_context_view();
        if (!c->capture) {
            c->i--;
            /* pt_block shouldn't change after capture */
            /* but the following should be implicit */
            /* c->bp->body[c->i] = ch; */
            return 0;
        }
    }
    return ungetc(ch, fh);
}

int pt_putc_escaped(char ch, FILE* fh) {
    switch (ch) {
    case '\n':
        fputs("\\n", fh);
        break;
    case '"':
    case '\\':
        putc('\\', fh);
        /* fall through */
    default:
        putc(ch, fh);
        break;
    }
}

int pt_putc(char ch, FILE* fh) {
    if (pt_cx) {
        struct pt_context* c = pt_context_view();
        if (c->capture) {
            c->bp->body[c->i] = ch;
            c->i++;
            return 0;
        }
        else if (c->escape) {
            return pt_putc_escaped(ch, fh);
        }
    }
    return putc(ch, fh);
}


void pt_skipchar(char ch, FILE* fh);
int pt_skipspaces(FILE* fh);

int pt_capture(const char *tok, FILE* fh) {
    if (pt_cx) {
        struct pt_context* c = pt_context_view();
        if (c->capture) {
            if (strcmp(tok, "end") == 0) {
                if (c->bp->type == PT_FOREACH) {
                    /* start expanding */
                    c->capture = 0;
                    c->bp->body[c->i++] = 0;
                    c->bp->body[c->i++] = -1;
                    c->i = 0;
                    return 0;
                } else {
                    pt_context_pop();
                    pt_skipspaces(fh);
                    return 0;
                }
            }
            if (c->bp->type == PT_LIST && strcmp(tok, "and") == 0) {
                c->bp->body[c->i++] = 0;
                c->bp->body[c->i++] = -1;
                c->i = ((c->i + PT_LISTITEM - 1) / PT_LISTITEM) * PT_LISTITEM;
                pt_skipspaces(fh);
                return 0;
            }
            char ch = 0;
            c->bp->body[c->i++] = '\\';
            while (1) {
                ch = *tok;
                if (!ch)
                    break;
                c->bp->body[c->i++] = ch;
                tok++;
            }
            return 0;
        }
    }
    return 1;
}

int pt_define(const char* name_, FILE* fh);
int pt_expand(const char* name);
int pt_foreach(FILE* fh);
const char* pt_getid(FILE* fh);

void pt_context_escape(FILE* infh, FILE* outfh) {
    pt_skipspaces(infh);
    pt_skipchar('\\', infh);
    const char* id = pt_getid(infh);
    if (pt_expand(id) == 0) {
        putc('"', outfh);
        struct pt_context *c = pt_context_view();
        c->escape = 1;
        return;
    }
    pt_putc('\\', outfh);
    pt_putc('s', outfh);
    pt_putc('\\', outfh);
    if (pt_cx) {
        struct pt_context *c = pt_context_view();
        int n = strlen(id) + 1;
        memcpy(c->bp->body + c->i, id, n);
        c->i += n;
    } else {
        fputs(id, outfh);
    }
}


enum pt_return
preprocess(const char* input, const char* output, int force) {
    printf("PP %s <- %s", output, input);
    fflush(stdout);

    if (!pt_need_build(input, output) && !force) {
        puts("  skip.");
        return SKIPPED;
    }

    FILE* infh = fopen(input, "r");
    if (!infh) {
        int e = errno;
        puts("  no.");
        fprintf(stderr, "Error: input: %s: %s\n", input, strerror(e));
        return FAILED;
    }
    // use temporary file for output, rename it to output on success
    FILE* outfh = fopen("preprocess.tmp", "w");
    if (!outfh) {
        int e = errno;
        puts("  no.");
        fprintf(stderr, "Error: output: %s: %s\n", output, strerror(e));
        fclose(infh);
        return FAILED;
    }
    enum {MACRO, ECHO} mode = ECHO;
    char ch = 0;
    const char* tok = 0;
    int escape = 0;
    while (1) {
        ch = pt_getc(infh);
        escape = 0;
        if (pt_cx && pt_context_view(pt_cx)->escape) {
            escape = 1;
        }
        if (ch == 0)
            ch = pt_getc(infh);
        if (ch < 0)
            break;
        if (escape && (!pt_cx || !pt_context_view(pt_cx)->escape)) {
            /* end of escape */
            pt_putc('"', outfh);
        }
        switch (mode) {
        case ECHO:
            if (ch != '\\') {
                pt_putc(ch, outfh);
                break;
            }
            else if (pt_skipspaces(infh))
                break;
            mode = MACRO;
            /* fall through */
        case MACRO:
            tok = pt_getid(infh);
            if (!tok) {
                puts("  no.");
                fclose(infh);
                fclose(outfh);
                unlink("preprocess.tmp");
                return FAILED;
            }
            else if(pt_capture(tok, infh) == 0)
                (void)0;
            else if(strcmp(tok, "s") == 0)
                pt_context_escape(infh, outfh);
            else if(strcmp(tok, "def") == 0 ||
                    strcmp(tok, "defs") == 0)
                pt_define(tok, infh);
            else if (pt_expand(tok) == 0)
                (void)0;
            else if (strcmp(tok, "foreach") == 0) {
                if (pt_foreach(infh)) {
                    fclose(infh);
                    fclose(outfh);
                    unlink("preprocess.tmp");
                    return FAILED;
                }
            }
            else {
                pt_putc('\\', outfh);
                fputs(tok, outfh);
            }
            mode = ECHO;
            break;
        }
    }
    puts("  ok.");
    fclose(infh);
    fclose(outfh);
    rename("preprocess.tmp", output);
    return SUCCESS;
}


int pt_skipspaces(FILE* fh) {
    char ch = 0;
    int skipped = 0;
    ch = pt_getc(fh);
    if (isspace(ch))
        skipped = 1;
    while (isspace(ch)) {
        ch = pt_getc(fh);
    }
    if (ch >= 0)
        pt_ungetc(ch, fh);
    return skipped;
}

void pt_skipchar(char ch_, FILE* fh) {
    char ch = 0;
    ch = pt_getc(fh);
    if (ch == ch_)
        return;
    if (ch >= 0)
        pt_ungetc(ch, fh);
}

int pt_define(const char* name_, FILE* fh) {
    pt_context_push(pt_bx, 1, 0, 0);
    struct pt_context* ctx = pt_context_view();
    char* name = ctx->bp->name;
    if (strcmp(name_, "defs") == 0)
        ctx->bp->type = PT_LIST;
    else
        ctx->bp->type = PT_ITEM;
    pt_skipspaces(fh);
    pt_skipchar('\\', fh);
    const char* id = pt_getid(fh);
    if (!id)
        return 1;
    int n = strlen(id) + 1;
    memcpy(name, id, n);
    pt_bx++;
    pt_skipspaces(fh);
    return 0;
}

int pt_expand(const char* name) {
    int bi = 0;
    int escape = 0;
    if (pt_cx) {
        struct pt_context *c = pt_context_view();
        if (c->bp->type == PT_FOREACH) {
            if (strcmp(c->bp->name, name) == 0) {
                const char* list = c->bp->name + 64;
                for (bi = 0; bi < pt_bx; bi++) {
                    if (strcmp(pt_blocks[bi].name, list) == 0) {
                        if (pt_blocks[bi].body[c->repeat * PT_LISTITEM]) {
                            pt_context_push(bi, 0, c->repeat, c->escape);
                            return 0;
                        }
                    }
                }
            }
        }
        escape = c->escape;
    }
    for (bi = 0; bi < pt_bx; bi++) {
        if (strcmp(pt_blocks[bi].name, name) == 0) {
            pt_context_push(bi, 0, 0, escape);
            return 0;
        }
    }
    return 1;
}

int pt_blocks_find(const char* id) {
    int bi = 0;
    for (bi = 0; bi < pt_bx; bi++) {
        if (strcmp(pt_blocks[bi].name, id) == 0) {
            return bi;
        }
    }
    return -1;
}

int pt_foreach(FILE* fh) {
    pt_context_push(pt_bx, 1, 0, 0);
    struct pt_context* ctx = pt_context_view();
    char* name = ctx->bp->name;
    pt_skipspaces(fh);
    pt_skipchar('\\', fh);
    ctx->bp->type = PT_FOREACH;
    const char* id = pt_getid(fh);
    if (!id)
        return 1;
    int n = strlen(id) + 1;
    memcpy(name, id, n);
    pt_skipspaces(fh);

    pt_skipspaces(fh);
    pt_skipchar('\\', fh);
    name = ctx->bp->name + ((n + 63) / 64) * 64;
    id = pt_getid(fh);
    if (strcmp(id, "in") == 0) {
        pt_skipspaces(fh);
        pt_skipchar('\\', fh);
        id = pt_getid(fh);
    }
    if (!id)
        return 1;
    if (pt_blocks_find(id) < 0) {
        puts(" no.");
        fflush(stdout);
        fprintf(stderr, "Error: foreach: Can't find: %s\n", id);
        return 1;
    }
    n = strlen(id) + 1;
    memcpy(name, id, n);

    pt_bx++;
    return 0;
}


const char* pt_getid(FILE* fh) {
    static char id[128];
    int i = 0;
    char ch;
    /* return if first char is not good */
    ch = pt_getc(fh);
    if (!isalpha(ch) && ch != '_') {
        if (ch > 0)
            id[i++] = ch;
        id[i++] = 0;
        return id;
    }
    while (ch > 0) {
        if (!isalnum(ch) && ch != '_')
            break;
        /* TODO: bounds check*/
        id[i++] = ch;
        ch = pt_getc(fh);
    }
    if (ch > 0)
        pt_ungetc(ch, fh);
    id[i++] = 0;
    return id;
}


#endif /* PTECH_IMPLEMENTATION */
#endif /* _PTECH_H_ */
