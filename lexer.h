#ifndef GMQCC_LEXER_HDR_
#define GMQCC_LEXER_HDR_

typedef struct token_s token;

#include "ast.h"

struct token_s {
	int ttype;

	MEM_VECTOR_MAKE(char, value);

	union {
		vector v;
		int    i;
		double f;
		int    t; /* type */
	} constval;

#if 0
	struct token_s *next;
	struct token_s *prev;
#endif

	lex_ctx ctx;
};

#if 0
token* token_new();
void   token_delete(token*);
token* token_copy(const token *cp);
void   token_delete_all(token *t);
token* token_copy_all(const token *cp);
#endif

/* Lexer
 *
 */
enum {
    /* Other tokens which we can return: */
    TOKEN_NONE = 0,
    TOKEN_START = 128,

    TOKEN_IDENT,

    TOKEN_TYPENAME,

    TOKEN_OPERATOR,

    TOKEN_KEYWORD, /* loop */

    TOKEN_DOTS, /* 3 dots, ... */

    TOKEN_STRINGCONST, /* not the typename but an actual "string" */
    TOKEN_CHARCONST,
    TOKEN_VECTORCONST,
    TOKEN_INTCONST,
    TOKEN_FLOATCONST,

    TOKEN_EOF,

    /* We use '< TOKEN_ERROR', so TOKEN_FATAL must come after it and any
     * other error related tokens as well
     */
    TOKEN_ERROR,
    TOKEN_FATAL /* internal error, eg out of memory */
};

static const char *_tokennames[] = {
    "TOKEN_START",
    "TOKEN_IDENT",
    "TOKEN_TYPENAME",
    "TOKEN_OPERATOR",
    "TOKEN_KEYWORD",
    "TOKEN_DOTS",
    "TOKEN_STRINGCONST",
    "TOKEN_CHARCONST",
    "TOKEN_VECTORCONST",
    "TOKEN_INTCONST",
    "TOKEN_FLOATCONST",
    "TOKEN_EOF",
    "TOKEN_ERROR",
    "TOKEN_FATAL",
};
typedef int
_all_tokennames_added_[
	((TOKEN_FATAL - TOKEN_START + 1) ==
	 (sizeof(_tokennames)/sizeof(_tokennames[0])))
	? 1 : -1];

typedef struct {
    char *name;
    int   value;
} frame_macro;

typedef struct {
	FILE   *file;
	char   *name;
	size_t  line;
	size_t  sline; /* line at the start of a token */

	char    peek[256];
	size_t  peekpos;

	bool    eof;

	token   tok; /* not a pointer anymore */

	struct {
	    bool noops;
	    bool nodigraphs; /* used when lexing string constants */
	} flags;

    int framevalue;
	MEM_VECTOR_MAKE(frame_macro, frames);
	char *modelname;
} lex_file;

MEM_VECTOR_PROTO(lex_file, char, token);

lex_file* lex_open (const char *file);
void      lex_close(lex_file   *lex);
int       lex_do   (lex_file   *lex);
void      lex_cleanup(void);

/* Parser
 *
 */

enum {
    ASSOC_LEFT,
    ASSOC_RIGHT
};

#define OP_SUFFIX 1
#define OP_PREFIX 2

typedef struct {
    const char   *op;
    unsigned int operands;
    unsigned int id;
    unsigned int assoc;
    unsigned int prec;
    unsigned int flags;
} oper_info;

#define opid1(a) (a)
#define opid2(a,b) ((a<<8)|b)
#define opid3(a,b,c) ((a<<16)|(b<<8)|c)

static const oper_info c_operators[] = {
    { "(",   0, opid1('('),         ASSOC_LEFT,  99, OP_PREFIX}, /* paren expression - non function call */

    { "++",  1, opid3('S','+','+'), ASSOC_LEFT,  16, OP_SUFFIX},
    { "--",  1, opid3('S','-','-'), ASSOC_LEFT,  16, OP_SUFFIX},

    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, 0 }, /* function call */

    { "!",   1, opid2('!', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "~",   1, opid2('~', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "+",   1, opid2('+','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",   1, opid2('-','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "++",  1, opid3('+','+','P'), ASSOC_RIGHT, 14, OP_PREFIX },
    { "--",  1, opid3('-','-','P'), ASSOC_RIGHT, 14, OP_PREFIX },
/*  { "&",   1, opid2('&','P'),     ASSOC_RIGHT, 14, OP_PREFIX }, */

    { "*",   2, opid1('*'),         ASSOC_LEFT,  13, 0 },
    { "/",   2, opid1('/'),         ASSOC_LEFT,  13, 0 },
    { "%",   2, opid1('%'),         ASSOC_LEFT,  13, 0 },

    { "+",   2, opid1('+'),         ASSOC_LEFT,  12, 0 },
    { "-",   2, opid1('-'),         ASSOC_LEFT,  12, 0 },

    { "<<",  2, opid2('<','<'),     ASSOC_LEFT,  11, 0 },
    { ">>",  2, opid2('>','>'),     ASSOC_LEFT,  11, 0 },

    { "<",   2, opid1('<'),         ASSOC_LEFT,  10, 0 },
    { ">",   2, opid1('>'),         ASSOC_LEFT,  10, 0 },
    { "<=",  2, opid2('<','='),     ASSOC_LEFT,  10, 0 },
    { ">=",  2, opid2('>','='),     ASSOC_LEFT,  10, 0 },

    { "==",  2, opid2('=','='),     ASSOC_LEFT,  9,  0 },
    { "!=",  2, opid2('!','='),     ASSOC_LEFT,  9,  0 },

    { "&",   2, opid1('&'),         ASSOC_LEFT,  8,  0 },

    { "^",   2, opid1('^'),         ASSOC_LEFT,  7,  0 },

    { "|",   2, opid1('|'),         ASSOC_LEFT,  6,  0 },

    { "&&",  2, opid2('&','&'),     ASSOC_LEFT,  5,  0 },

    { "||",  2, opid2('|','|'),     ASSOC_LEFT,  4,  0 },

    { "?",   3, opid2('?',':'),     ASSOC_RIGHT, 3,  0 },

    { "=",   2, opid1('='),         ASSOC_RIGHT, 2,  0 },
    { "+=",  2, opid2('+','='),     ASSOC_RIGHT, 2,  0 },
    { "-=",  2, opid2('-','='),     ASSOC_RIGHT, 2,  0 },
    { "*=",  2, opid2('*','='),     ASSOC_RIGHT, 2,  0 },
    { "/=",  2, opid2('/','='),     ASSOC_RIGHT, 2,  0 },
    { "%=",  2, opid2('%','='),     ASSOC_RIGHT, 2,  0 },
    { ">>=", 2, opid3('>','>','='), ASSOC_RIGHT, 2,  0 },
    { "<<=", 2, opid3('<','<','='), ASSOC_RIGHT, 2,  0 },
    { "&=",  2, opid2('&','='),     ASSOC_RIGHT, 2,  0 },
    { "^=",  2, opid2('^','='),     ASSOC_RIGHT, 2,  0 },
    { "|=",  2, opid2('|','='),     ASSOC_RIGHT, 2,  0 },

    { ",",   2, opid1(','),         ASSOC_LEFT,  1,  0 }
};
static const size_t c_operator_count = (sizeof(c_operators) / sizeof(c_operators[0]));

static const oper_info qcc_operators[] = {
    { "(",   0, opid1('('),         ASSOC_LEFT,  99, OP_PREFIX}, /* paren expression - non function call */

    { ".",   2, opid1('.'),         ASSOC_LEFT,  15, 0 },
    { "(",   0, opid1('('),         ASSOC_LEFT,  15, 0 }, /* function call */

    { "!",   1, opid2('!', 'P'),    ASSOC_RIGHT, 14, OP_PREFIX },
    { "+",   1, opid2('+','P'),     ASSOC_RIGHT, 14, OP_PREFIX },
    { "-",   1, opid2('-','P'),     ASSOC_RIGHT, 14, OP_PREFIX },

    { "*",   2, opid1('*'),         ASSOC_LEFT,  13, 0 },
    { "/",   2, opid1('/'),         ASSOC_LEFT,  13, 0 },
    { "&",   2, opid1('&'),         ASSOC_LEFT,  13, 0 },
    { "|",   2, opid1('|'),         ASSOC_LEFT,  13, 0 },

    { "+",   2, opid1('+'),         ASSOC_LEFT,  12, 0 },
    { "-",   2, opid1('-'),         ASSOC_LEFT,  12, 0 },

    { "<",   2, opid1('<'),         ASSOC_LEFT,  10, 0 },
    { ">",   2, opid1('>'),         ASSOC_LEFT,  10, 0 },
    { "<=",  2, opid2('<','='),     ASSOC_LEFT,  10, 0 },
    { ">=",  2, opid2('>','='),     ASSOC_LEFT,  10, 0 },
    { "==",  2, opid2('=','='),     ASSOC_LEFT,  10,  0 },
    { "!=",  2, opid2('!','='),     ASSOC_LEFT,  10,  0 },

    { "=",   2, opid1('='),         ASSOC_RIGHT, 8,  0 },
    { "+=",  2, opid2('+','='),     ASSOC_RIGHT, 8,  0 },
    { "-=",  2, opid2('-','='),     ASSOC_RIGHT, 8,  0 },
    { "*=",  2, opid2('*','='),     ASSOC_RIGHT, 8,  0 },
    { "/=",  2, opid2('/','='),     ASSOC_RIGHT, 8,  0 },
    { "%=",  2, opid2('%','='),     ASSOC_RIGHT, 8,  0 },
    { "&=",  2, opid2('&','='),     ASSOC_RIGHT, 8,  0 },
    { "|=",  2, opid2('|','='),     ASSOC_RIGHT, 8,  0 },

    { "&&",  2, opid2('&','&'),     ASSOC_LEFT,  5,  0 },
    { "||",  2, opid2('|','|'),     ASSOC_LEFT,  5,  0 },

    { ",",   2, opid1(','),         ASSOC_LEFT,  1,  0 }
};
static const size_t qcc_operator_count = (sizeof(qcc_operators) / sizeof(qcc_operators[0]));

extern const oper_info *operators;
extern size_t           operator_count;
void lexerror(lex_file*, const char *fmt, ...);

#endif
