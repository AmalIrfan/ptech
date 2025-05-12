/* This is an example program that uses ptech
   It reads op and writes opcode */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum op {
    OP_RET,
    OP_PHA,
    OP_PLA,

    OP_JSR,
    OP_BEQ,
    OP_ADC,
    OP_SBC,

    OP_RJSR,
};

int rop[] = {
    [OP_JSR] = OP_RJSR,
};

struct input {
    union {
        const char *str;
        FILE *fh; 
    } input;
    enum {
        STRING,
        FILE_,
    } type;
};

int assemble(struct input input);

int main()
{
    struct input input = {
        .input = {.fh = stdin},
        .type = FILE_,
    };
    int opcode = assemble(input);
    fputs((void*)&opcode, stdout);
}

union token {
    char str[10];
    int value;
};

union token gettok(struct input *input);

int makeupper(char *str);
int makenumber(const char *str);
int makehexa(const char *str);

int assemble(struct input input) {
    union token op = gettok(&input);
    union token arg = {0};

    int opcode = 0;

    makeupper(op.str);

    if (strcmp(op.str, "RET") == 0)
        opcode = OP_RET;
    else if (strcmp(op.str, "PHA") == 0)
        opcode = OP_PHA;
    else if (strcmp(op.str, "PLA") == 0)
        opcode = OP_PLA;
    else if (strcmp(op.str, "JSR") == 0) {
        opcode = OP_JSR;
    } else if (strcmp(op.str, "BEQ") == 0) {
        opcode = OP_BEQ;
    } else if (strcmp(op.str, "ADC") == 0) {
        opcode = OP_ADC;
    } else if (strcmp(op.str, "SBC") == 0) {
        opcode = OP_SBC;
    } else
        return -1;

    switch (opcode) {
    case OP_JSR:
    case OP_BEQ:
    case OP_ADC:
    case OP_SBC:
        arg = gettok(&input);
        if (arg.str[0] == '$') {
            opcode = rop[opcode];
        }
        if (arg.str[0] == '#' || arg.str[0] == '$' && arg.str[1] == '#')
            arg.value = makehexa(arg.str);
        else
            arg.value = makenumber(arg.str);
        opcode |= arg.value << 8;
        break;
    }

    return opcode;
}

#105 "simple.t"

union token gettok(struct input *in)
{
    char ch = 0;
    union token token;
    int i = 0;
    do {
        if (in->type == STRING)
            ch = *in->input.str++;
        else if (in->type == FILE_)
            ch = getc(in->input.fh);
        else
            break;
        if (!isspace(ch))
            break;
    } while(ch);

    while(ch) {
        token.str[i++] = ch;
        if (in->type == STRING)
            ch = *in->input.str++;
        else if (in->type == FILE_)
            ch = getc(in->input.fh);
        else
            break;
        if (isspace(ch))
            break;
    };
    token.str[i++] = 0;
    return token;
}

int makeupper(char *str) {
    char ch = *str;
    while (ch) {
    	if (isalpha(ch))
	    *str = ch & 95;
	ch = *++str;
    }
    return 0;
}

int makenumber(const char *str) {
    int n = 0;
    char ch = *str;
    if (ch == '$')
        ch = *++str;
    do {
        n = n * 10 + ch - '0';
        ch = *++str;
    } while(ch);
    return n;
}

int makehexa(const char *str) {
    int n = 0;
    char ch = *++str;
    if (ch == '#')
        ch = *++str;
    do {
        if (ch < '9')
            n = n * 16 + ch - '0';
        else
            n = n * 16 + 10 + ch - 'A';
        ch = *++str;
    } while(ch);
    return n;
}
