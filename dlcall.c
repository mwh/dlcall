// dlcall - call a library function from the command line
// Copyright (C) 2018 Michael Homer
// Distributed under the GNU GPL version 3+.
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define ARG_DEFAULT 0
#define ARG_STRING 1
#define ARG_INT 2
#define ARG_DOUBLE 3
#define ARG_CHAR 4

int verbose = 0;

union argvalue {
    char *s;
    int i;
    double d;
    float f;
    char c;
};

struct argument {
    int type;
    union argvalue value;
};

struct argument *get_arguments(int *size, int *return_type, int argc,
        char **argv) {
    struct argument *ret = malloc(sizeof(struct argument) * argc);
    int arg = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp("-r", argv[i]) == 0) {
            i++;
            if (argv[i][0] == 's')
                *return_type = ARG_STRING;
            if (argv[i][0] == 'd')
                *return_type = ARG_DOUBLE;
            if (argv[i][0] == 'i')
                *return_type = ARG_INT;
            if (argv[i][0] == 'c')
                *return_type = ARG_CHAR;
            continue;
        }
        if (strcmp("-c", argv[i]) == 0) {
            i++;
            ret[arg].type = ARG_CHAR;
            ret[arg++].value.c = argv[i][0];
            continue;
        }
        if (strcmp("-s", argv[i]) == 0) {
            i++;
            ret[arg].type = ARG_STRING;
            ret[arg++].value.s = argv[i];
            continue;
        }
        if (strcmp("-i", argv[i]) == 0) {
            i++;
            ret[arg].type = ARG_INT;
            ret[arg++].value.i = atoi(argv[i]);
            continue;
        }
        if (strcmp("-d", argv[i]) == 0) {
            i++;
            ret[arg].type = ARG_DOUBLE;
            ret[arg++].value.d = atof(argv[i]);
            continue;
        }
        int ival = atoi(argv[i]);
        double dval = atof(argv[i]);
        if (((argv[i][0] >= '0' && argv[i][0] <= '9') || argv[i][0] == '-') &&
                strchr(argv[i], '.')) {
            ret[arg].type = ARG_DOUBLE;
            ret[arg].value.d = dval;
            arg++;
        } else if (ival != 0 || argv[i][0] == '0') {
            ret[arg].type = ARG_INT;
            ret[arg].value.i = ival;
            arg++;
        } else {
            ret[arg].type = ARG_STRING;
            ret[arg].value.s = argv[i];
            arg++;
        }
    }
    *size = arg;
    return ret;
}

char *describe_function(char *name, struct argument *args, int argc,
        int return_type) {
    char *ret = malloc(argc * 7 + strlen(name) + 2 + 7);
    char *pos = ret;
    ret[0] = 0;
    switch(return_type) {
        case ARG_INT: pos = stpcpy(ret, "int "); break;
        case ARG_DOUBLE: pos = stpcpy(ret, "double "); break;
        case ARG_STRING: pos = stpcpy(ret, "char *"); break;
        case ARG_CHAR: pos = stpcpy(ret, "char "); break;
    }
    pos = stpcpy(pos, name);
    pos = stpcpy(pos, "(");
    for (int i = 0; i < argc; i++) {
        if (i > 0) {
            *(pos++) = ',';
        }
        switch (args[i].type) {
            case ARG_INT: strcpy(pos, "int"); pos += 3; break;
            case ARG_DOUBLE: strcpy(pos, "double"); pos += 6; break;
            case ARG_STRING: strcpy(pos, "char*"); pos += 5; break;
            case ARG_CHAR: strcpy(pos, "char"); pos += 4; break;
        }
        *pos = 0;
    }
    pos = stpcpy(pos, ")");
    return ret;
}

void log_verbose(char *msg, ...) {
    if (!verbose)
        return;
    va_list args;
    va_start(args, msg);
    fprintf(stderr, "dlcall: ");
    vfprintf(stderr, msg, args);
    fputs("\n", stderr);
    va_end(args);
}

// Macros for defining repetitive prototypes below
#define type_STRING char*
#define type_INT int
#define type_CHAR char
#define type_DOUBLE double
#define access_STRING .value.s
#define access_DOUBLE .value.d
#define access_CHAR .value.c
#define access_INT .value.i
#define format_INT "%i"
#define format_STRING "%s"
#define format_DOUBLE "%f"
#define NULLARY(rtype) if (return_type == ARG_ ## rtype) {\
    type_ ## rtype (*f)() = sym; \
    type_ ## rtype ret = f(); \
    printf(format_ ## rtype "\n", ret); \
    return 1; }
#define UNARY(arg1,rtype) if (args[0].type == ARG_ ## arg1 && return_type == ARG_ ## rtype) {\
    type_ ## rtype (*f)(type_ ## arg1) = sym; \
    type_ ## rtype ret = f(args[0] access_ ## arg1); \
    printf(format_ ## rtype "\n", ret); \
    return 1; }
#define BINARY(arg1,arg2,rtype) if (args[0].type == ARG_ ## arg1 && args[1].type == ARG_ ## arg2 && return_type == ARG_ ## rtype) {\
    type_ ## rtype (*f)(type_ ## arg1, type_ ## arg2) = sym; \
    type_ ## rtype ret = f(args[0] access_ ## arg1,args[1] access_ ## arg2); \
    printf(format_ ## rtype "\n", ret); \
    return 1; }
#define TERNARY(arg1,arg2,arg3,rtype) if (args[0].type == ARG_ ## arg1 && args[1].type == ARG_ ## arg2 && args[2].type == ARG_ ## arg3 && return_type == ARG_ ## rtype) {\
    type_ ## rtype (*f)(type_ ## arg1, type_ ## arg2, type_ ## arg3) = sym; \
    type_ ## rtype ret = f(args[0] access_ ## arg1,args[1] access_ ## arg2, args[2] access_ ## arg3); \
    printf(format_ ## rtype "\n", ret); \
    return 1; }
#define UNABLE(s) "notice: dlcall does not know how to handle " s " prototype %s. Pull requests are welcome at <https://github.com/mwh/dlcall>."
bool try(void *handle, char *symbol, int argc, char **argv) {
    void *sym = dlsym(handle, symbol);
    if (!sym) {
        log_verbose("no such symbol %s.", symbol);
        return 0;
    }
    int arg_size = 0;
    int return_type = 0;
    struct argument *args = get_arguments(&arg_size, &return_type, argc, argv);
    // If any argument is a double, default the return type to double.
    // Otherwise, assume it's an int.
    if (!return_type) {
        return_type = ARG_INT;
        for (int i = 0; i < arg_size; i++)
            if (args[i].type == ARG_DOUBLE)
                return_type = ARG_DOUBLE;
    }
    if (arg_size == 0) {
        NULLARY(STRING);
        NULLARY(INT);
        NULLARY(DOUBLE);
        log_verbose(UNABLE("nullary"),
                describe_function(symbol, args, arg_size, return_type));
    }
    else if (arg_size == 1) {
        UNARY(STRING,INT);
        UNARY(STRING, STRING);
        UNARY(DOUBLE, DOUBLE);
        UNARY(INT, INT);
        UNARY(CHAR, INT);
        log_verbose(UNABLE("unary"),
                describe_function(symbol, args, arg_size, return_type));
    } else if (arg_size == 2) {
        BINARY(STRING, INT, INT);
        BINARY(STRING, STRING, INT);
        BINARY(STRING, STRING, STRING);
        BINARY(STRING, CHAR, STRING);
        BINARY(DOUBLE, DOUBLE, DOUBLE);
        BINARY(STRING, CHAR, STRING);
        BINARY(DOUBLE, INT, DOUBLE);
        BINARY(INT, STRING, INT);
        log_verbose(UNABLE("binary"),
                describe_function(symbol, args, arg_size, return_type));
    } else if (arg_size == 3) {
        TERNARY(INT, STRING, INT, INT);
        log_verbose(UNABLE("ternary"),
                describe_function(symbol, args, arg_size, return_type));
    } else if (verbose) {
        fprintf(stderr, "dlcall: notice: unable to handle arity %i.\n",
                arg_size);
    }
    return 0;
}

int main(int argc, char **argv) {
    int arg_size;
    int return_type;
    struct argument *args;
    if (argc == 1 || strcmp(argv[1], "--help") == 0
            || strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [library] function arguments...\n", argv[0]);
        puts("The first argument is either a function name or the dynamic");
        puts("library to load the function from. All subsequent arguments");
        puts("describe the arguments and function prototype.");
        puts("");
        puts("    -s ARG    ARG is a string argument");
        puts("    -i ARG    ARG is an int argument");
        puts("    -c ARG    ARG is a char argument");
        puts("    -d ARG    ARG is a double argument");
        puts("    -r [sicd] function returns string, int, char, or double");
        puts("    -v        Enable verbose logging");
        puts("");
        puts("Examples:");
        puts("    dlcall sin 2.5");
        puts("    dlcall strlen \"hello world\"");
        puts("    dlcall strcasecmp hello HELLO");
        puts("    dlcall strchr world -c r -r s");
        puts("    dlcall getenv HOME -r s");
        puts("    dlcall write 1 hello 5");
        puts("    dlcall isalpha -c 6");
        return 0;
    }
    char **arg_start = argv + 1;
    int nargs = argc - 1;
    if (strcmp(argv[1], "-v") == 0) {
        verbose = 1;
        arg_start++;
        nargs--;
    }
    if (!try(RTLD_DEFAULT, arg_start[0], nargs - 1, arg_start + 1)) {
        void *handle = dlopen(arg_start[0], RTLD_LAZY);
        if (!handle) {
            args = get_arguments(&arg_size, &return_type, nargs-1, arg_start+1);
            fprintf(stderr, "%s: error: Could not call function %s or load %s "
                    "as a library.\n",
                    argv[0],
                    describe_function(arg_start[0], args, arg_size,return_type),
                    arg_start[0]);
            return 1;
        }
        log_verbose("loaded %s as a library.", arg_start[0]);
        if (!try(handle, arg_start[1], nargs - 2, arg_start + 1)) {
            args = get_arguments(&arg_size, &return_type, nargs-2, arg_start+2);
            fprintf(stderr, "%s: error: Could not call function %s from %s.\n",
                    argv[0],
                    describe_function(arg_start[1], args, arg_size,return_type),
                    argv[1]);
            return 1;
        }
    }
    return 0;
}
