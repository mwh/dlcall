// dlcall - call a library function from the command line
// Copyright (C) 2018 Michael Homer
// Distributed under the GNU GPL version 3+.
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define ARG_STRING 1
#define ARG_INT 2
#define ARG_DOUBLE 3
#define ARG_CHAR 4

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

bool try(void *handle, const char *symbol, int argc, char **argv) {
    void *sym = dlsym(handle, symbol);
    int arg_size = 0;
    int return_type = 0;
    struct argument *args = get_arguments(&arg_size, &return_type, argc, argv);
    if (arg_size == 1) {
        if (args[0].type == ARG_STRING && (return_type == ARG_INT
                    || !return_type)) {
            int(*f)(char *) = sym;
            int ret = f(args[0].value.s);
            printf("%i\n", ret);
            return 1;
        }
        if (args[0].type == ARG_DOUBLE && (return_type == ARG_DOUBLE
                || !return_type)) {
            double(*f)(double) = sym;
            double ret = f(args[0].value.d);
            printf("%f\n", ret);
            return 1;
        }
    } else if (arg_size == 2) {
        if (args[0].type == ARG_STRING && args[1].type == ARG_INT &&
                (return_type == ARG_INT
                    || !return_type)) {
            int(*f)(char *, int) = sym;
            int ret = f(args[0].value.s, args[1].value.i);
            printf("%i\n", ret);
            return 1;
        }
        if (args[0].type == ARG_STRING && args[1].type == ARG_STRING &&
                return_type == ARG_STRING) {
            char *(*f)(char *, char *) = sym;
            char * ret = f(args[0].value.s, args[1].value.s);
            printf("%s\n", ret);
            return 1;
        }
        if (args[0].type == ARG_STRING && args[1].type == ARG_CHAR &&
                return_type == ARG_STRING) {
            char *(*f)(char *, char) = sym;
            char * ret = f(args[0].value.s, args[1].value.c);
            printf("%s\n", ret);
            return 1;
        }
        if (args[0].type == ARG_DOUBLE && args[1].type == ARG_DOUBLE &&
                (return_type == ARG_DOUBLE
                    || !return_type)) {
            double(*f)(double, double) = sym;
            double ret = f(args[0].value.d, args[1].value.d);
            printf("%f\n", ret);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (!try(RTLD_DEFAULT, argv[1], argc-2, argv + 2)) {
        fprintf(stderr, "Could not call function %s.\n", argv[1]);
    }
    return 0;
}
