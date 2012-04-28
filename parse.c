/*
 * Copyright (C) 2012
 *     Dale Weiler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gmqcc.h"

/* compile-time constant for type constants */
typedef struct {
    char *name;
    int   type;
    float value[3];
    char *string; /* string value if constant is string literal */
} constant;
VECTOR_MAKE(constant, compile_constants);

void compile_constant_debug() {
    int iter = 0;
    for(; iter < compile_constants_elements; iter++) {
        constant *c = &compile_constants_data[iter];
        switch(c->type) {
            case TYPE_FLOAT:  printf("constant: %s FLOAT   %f\n",       c->name, c->value[0]);                           break;
            case TYPE_VECTOR: printf("constant: %s VECTOR {%f,%f,%f}\n",c->name, c->value[0], c->value[1], c->value[2]); break;
            case TYPE_STRING: printf("constant: %s STRING  %s\n",       c->name, c->string); break;
            case TYPE_VOID:   printf("constant: %s VOID    %s\n",       c->name, c->string); break;
        }
    }
}

/*
 * Generates a parse tree out of the lexees generated by the lexer.  This
 * is where the tree is built.  This is where valid check is performed.
 */
int parse_gen(lex_file *file) {
    int     token = 0;
    while ((token = lex_token(file)) != ERROR_LEX && file->length >= 0) {
        switch (token) {
            case TOKEN_TYPEDEF: {
                char *f; /* from */
                char *t; /* to   */

                token = lex_token(file);
                token = lex_token(file); f = util_strdup(file->lastok);
                token = lex_token(file);
                token = lex_token(file); t = util_strdup(file->lastok);

                typedef_add(file, f, t);
                mem_d(f);
                mem_d(t);

                token = lex_token(file);
                if (token == ' ')
                    token = lex_token(file);

                if (token != ';')
                    error(file, ERROR_PARSE, "Expected a `;` at end of typedef statement");

                token = lex_token(file);
                break;
            }

            case TOKEN_VOID:   goto fall;
            case TOKEN_STRING: goto fall;
            case TOKEN_VECTOR: goto fall;
            case TOKEN_ENTITY: goto fall;
            case TOKEN_FLOAT:  goto fall;
            {
            fall:; {
                char *name = NULL;
                int   type = token; /* story copy */

                /* skip over space */
                token = lex_token(file);
                if (token == ' ')
                    token = lex_token(file);

                /* save name */
                name = util_strdup(file->lastok);

                /* skip spaces */
                token = lex_token(file);
                if (token == ' ')
                    token = lex_token(file);

                if (token == ';') {
                    /*
                     * Definitions go to the defs table, they don't have
                     * any sort of data with them yet.
                     */
                } else if (token == '=') {
                    token = lex_token(file);
                    if (token == ' ')
                        token = lex_token(file);

                    /* strings are in file->lastok */
                    switch (type) {
                        case TOKEN_VOID:
                            error(file, ERROR_PARSE, "Cannot assign value to type void\n");

                        /* TODO: Validate (end quote), strip quotes for constant add, name constant */
                        case TOKEN_STRING:
                            if (*file->lastok != '"')
                                error(file, ERROR_PARSE, "Expected a '\"' (quote) for string constant\n");
                            /* add the compile-time constant */
                            {
                                constant c;
                                c.name     = util_strdup(name),
                                c.type     = TYPE_STRING,
                                c.value[0] = 0;
                                c.value[1] = 0;
                                c.value[2] = 0;
                                c.string   = util_strdup(file->lastok);
                                compile_constants_add(c);
                            }
                            break;
                        /* TODO: name constant, old qc vec literals, whitespace fixes, name constant */
                        case TOKEN_VECTOR: {
                            float compile_calc_x = 0;
                            float compile_calc_y = 0;
                            float compile_calc_z = 0;
                            int   compile_calc_d = 0; /* dot?        */
                            int   compile_calc_s = 0; /* sign (-, +) */

                            char  compile_data[1024];
                            char *compile_eval = compile_data;

                            if (token != '{')
                                error(file, ERROR_PARSE, "Expected initializer list {} for vector constant\n");

                            /*
                             * This parses a single vector element: x,y & z.  This will handle all the
                             * complicated mechanics of a vector, and can be extended as well.  This
                             * is a rather large macro, and is #undef'd after it's use below.
                             */
                            #define PARSE_VEC_ELEMENT(NAME, BIT)                                                                                                           \
                                token = lex_token(file);                                                                                                                   \
                                if (token == ' ')                                                                                                                          \
                                    token = lex_token(file);                                                                                                               \
                                if (token == '.')                                                                                                                          \
                                    compile_calc_d = 1;                                                                                                                    \
                                if (!isdigit(token) && !compile_calc_d && token != '+' && token != '-')                                                                    \
                                    error(file, ERROR_PARSE,"Invalid constant initializer element %c for vector, must be numeric\n", NAME);                                \
                                if (token == '+')                                                                                                                          \
                                    compile_calc_s = '+';                                                                                                                  \
                                if (token == '-' && !compile_calc_s)                                                                                                       \
                                    compile_calc_s = '-';                                                                                                                  \
                                while (isdigit(token) || token == '.' || token == '+' || token == '-') {                                                                   \
                                    *compile_eval++ = token;                                                                                                               \
                                    token           = lex_token(file);                                                                                                     \
                                    if (token == '.' && compile_calc_d) {                                                                                                  \
                                        error(file, ERROR_PARSE, "Invalid constant initializer element %c for vector, must be numeric.\n", NAME);                          \
                                        token = lex_token(file);                                                                                                           \
                                    }                                                                                                                                      \
                                    if ((token == '-' || token == '+') && compile_calc_s) {                                                                                \
                                        error(file, ERROR_PARSE, "Invalid constant initializer sign for vector element %c\n", NAME);                                       \
                                        token = lex_token(file);                                                                                                           \
                                    }                                                                                                                                      \
                                    else if (token == '.' && !compile_calc_d)                                                                                              \
                                        compile_calc_d = 1;                                                                                                                \
                                    else if (token == '-' && !compile_calc_s)                                                                                              \
                                        compile_calc_s = '-';                                                                                                              \
                                    else if (token == '+' && !compile_calc_s)                                                                                              \
                                        compile_calc_s = '+';                                                                                                              \
                                }                                                                                                                                          \
                                if (token == ' ')                                                                                                                          \
                                    token = lex_token(file);                                                                                                               \
                                if (NAME != 'z') {                                                                                                                         \
                                    if (token != ',' && token != ' ')                                                                                                      \
                                        error(file, ERROR_PARSE, "invalid constant initializer element %c for vector (missing spaces, or comma delimited list?)\n", NAME); \
                                } else if (token != '}') {                                                                                                                 \
                                    error(file, ERROR_PARSE, "Expected `}` on end of constant initialization for vector\n");                                               \
                                }                                                                                                                                          \
                                compile_calc_##BIT = atof(compile_data);                                                                                                   \
                                compile_calc_d = 0;                                                                                                                        \
                                compile_calc_s = 0;                                                                                                                        \
                                compile_eval   = &compile_data[0];                                                                                                         \
                                memset(compile_data, 0, sizeof(compile_data))

                            /*
                             * Parse all elements using the macro above.
                             * We must undef the macro afterwards.
                             */
                            PARSE_VEC_ELEMENT('x', x);
                            PARSE_VEC_ELEMENT('y', y);
                            PARSE_VEC_ELEMENT('z', z);
                            #undef PARSE_VEC_ELEMENT

                            /* Check for the semi-colon... */
                            token = lex_token(file);
                            if (token == ' ')
                                token = lex_token(file);
                            if (token != ';')
                                error(file, ERROR_PARSE, "Expected `;` on end of constant initialization for vector\n");

                            /* add the compile-time constant */
                            {
                                constant c;
                                
                                c.name     = util_strdup(name),
                                c.type     = TYPE_VECTOR,
                                c.value[0] = compile_calc_x;
                                c.value[1] = compile_calc_y;
                                c.value[2] = compile_calc_z;
                                c.string   = NULL;
                                compile_constants_add(c);
                            }
                            break;
                        }

                        case TOKEN_ENTITY:
                        case TOKEN_FLOAT: /*TODO: validate, constant generation, name constant */
                            if (!isdigit(token))
                                error(file, ERROR_PARSE, "Expected numeric constant for float constant\n");
                            /* constant */
                            {
                                constant c;
                                c.name     = util_strdup(name),
                                c.type     = TOKEN_FLOAT,
                                c.value[0] = 0;
                                c.value[1] = 0;
                                c.value[2] = 0;
                                c.string   = NULL;
                                compile_constants_add(c);
                            }
                            break;
                    }
                } else if (token == '(') {
                    printf("FUNCTION ??\n");
                }
                mem_d(name);
            }}

            /*
             * From here down is all language punctuation:  There is no
             * need to actual create tokens from these because they're already
             * tokenized as these individual tokens (which are in a special area
             * of the ascii table which doesn't conflict with our other tokens
             * which are higer than the ascii table.)
             */
            case '#':
                token = lex_token(file); /* skip '#' */
                if (token == ' ')
                    token = lex_token(file);
                /*
                 * If we make it here we found a directive, the supported
                 * directives so far are #include.
                 */
                if (strncmp(file->lastok, "include", sizeof("include")) == 0) {
                    char     *copy = NULL;
                    lex_file *next = NULL;
                    /*
                     * We only suport include " ", not <> like in C (why?)
                     * because the latter is silly.
                     */
                    while (*file->lastok != '"' && token != '\n')
                        token = lex_token(file);
                    if (token == '\n')
                        return error(file, ERROR_PARSE, "Invalid use of include preprocessor directive: wanted #include \"file.h\"\n");

                    copy = util_strdup(file->lastok);
                    next = lex_include(file,   copy);

                    if (!next) {
                        error(file, ERROR_INTERNAL, "Include subsystem failure\n");
                        exit (-1);
                    }
                    /* constant */
                    {
                        constant c;
                        c.name     = "#include",
                        c.type     = TYPE_VOID,
                        c.value[0] = 0;
                        c.value[1] = 0;
                        c.value[2] = 0;
                        c.string   = copy;
                        compile_constants_add(c);
                    }
                    parse_gen(next);
                    mem_d    (copy);
                    lex_close(next);
                }
                /* skip all tokens to end of directive */
                while (token != '\n')
                    token = lex_token(file);
                break;

            case LEX_IDENT:
                token = lex_token(file);
                break;
        }
    }
    compile_constant_debug();
    lex_reset(file);
    /* free constants */
    {
        size_t i = 0;
        for (; i < compile_constants_elements; i++) {
            mem_d(compile_constants_data[i].name);
            mem_d(compile_constants_data[i].string);
        }
        mem_d(compile_constants_data);
    }
    return 1;
}
