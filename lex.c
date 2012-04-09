/*
 * Copyright (C) 2012 
 * 	Dale Weiler
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
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "gmqcc.h"

static const char *const lex_keywords[] = {
	"do",    "else",     "if",     "while",
	"break", "continue", "return", "goto",
	"for"
};

struct lex_file *lex_open(const char *name) {
	struct lex_file *lex = mem_a(sizeof(struct lex_file));
	if (lex) {
		lex->file = fopen(name, "r");
		fseek(lex->file, 0, SEEK_END);
		lex->length = ftell(lex->file);
		lex->size   = lex->length; /* copy, this is never changed */
		fseek(lex->file, 0, SEEK_SET);
		lex->last = 0;
		
		memset(lex->peek, 0, sizeof(lex->peek));
	}
	return lex;
}

int lex_close(struct lex_file *file) {
	int ret = -1;
	if (file) {
		ret = fclose(file->file);
		mem_d(file);
	}
	return ret;
}

static void lex_addch(int ch, struct lex_file *file) {
	if (file->current <  sizeof(file->lastok)-1)
		file->lastok[file->current++] = (char)ch;
	if (file->current == sizeof(file->lastok)-1)
		file->lastok[file->current]   = (char)'\0';
}
static inline void lex_clear(struct lex_file *file) {
	file->current = 0;
}

/*
 * read in inget/unget character from a lexer stream.
 * This doesn't play with file streams, the lexer has
 * it's own internal state for this.
 */
static int lex_inget(struct lex_file *file) {
	file->length --;
	if (file->last > 0)
		return file->peek[--file->last];
	return fgetc(file->file);
}
static void lex_unget(int ch, struct lex_file *file) {
	if (file->last < sizeof(file->peek))
		file->peek[file->last++] = ch;
	file->length ++;
}

/*
 * This is trigraph and digraph support, a feature not qc compiler
 * supports.  Moving up in this world!
 */
static int lex_trigraph(struct lex_file *file) {
	int  ch;
	if ((ch = lex_inget(file)) != '?') {
		lex_unget(ch, file);
		return '?';
	}
	
	ch = lex_inget(file);
	switch (ch) {
		case '(' : return '[' ;
		case ')' : return ']' ;
		case '/' : return '\\';
		case '\'': return '^' ;
		case '<' : return '{' ;
		case '>' : return '}' ;
		case '!' : return '|' ;
		case '-' : return '~' ;
		case '=' : return '#' ;
		default:
			lex_unget('?', file);
			lex_unget(ch , file);
			return '?';
	}
	return '?';
}
static int lex_digraph(struct lex_file *file, int first) {
	int ch = lex_inget(file);
	switch (first) {
		case '<':
			if (ch == '%') return '{';
			if (ch == ':') return '[';
			break;
		case '%':
			if (ch == '>') return '}';
			if (ch == ':') return '#';
			break;
		case ':':
			if (ch == '>') return ']';
			break;
	}
	
	lex_unget(ch, file);
	return first;
}

static int lex_getch(struct lex_file *file) {
	int ch = lex_inget(file);
	if (ch == '?')
		return lex_trigraph(file);
	if (ch == '<' || ch == ':' || ch == '%')
		return lex_digraph (file, ch);
		
	return ch;
}

static int lex_get(struct lex_file *file) {
	int ch;
	if (!isspace(ch = lex_getch(file)))
		return ch;
	
	/* skip over all spaces */
	while (isspace(ch) && ch != '\n')
		ch = lex_getch(file);
		
	if (ch == '\n')
		return ch;
		
	lex_unget(ch, file);
	return ' ';
}

static int lex_skipchr(struct lex_file *file) {
	int ch;
	int it;
	
	lex_clear(file);
	lex_addch('\'', file);
	
	for (it = 0; it < 2 && ((ch = lex_inget(file)) != '\''); it++) {
		lex_addch(ch, file);
		
		if (ch == '\n')
			return ERROR_LEX;
		if (ch == '\\')
			lex_addch(lex_getch(file), file);
	}
	lex_addch('\'', file);
	lex_addch('\0', file);
	
	if (it > 2)
		return ERROR_LEX;
		
	return LEX_CHRLIT;
}

static int lex_skipstr(struct lex_file *file) {
	int ch;
	lex_clear(file);
	lex_addch('"', file);
	
	while ((ch = lex_getch(file)) != '"') {
		if (ch == '\n' || ch == EOF)
			return ERROR_LEX;
			
		lex_addch(ch, file);
		if (ch == '\\')
			lex_addch(lex_inget(file), file);
	}
	
	lex_addch('"', file);
	lex_addch('\0', file);
	
	return LEX_STRLIT;
}
static int lex_skipcmt(struct lex_file *file) {
	int ch;
	lex_clear(file);
	ch = lex_getch(file);
	
	if (ch == '/') {
		lex_addch('/', file);
		lex_addch('/', file);
		
		while ((ch = lex_getch(file)) != '\n') {
			if (ch == '\\') {
				lex_addch(ch, file);
				lex_addch(lex_getch(file), file);
			} else {
				lex_addch(ch, file);
			}
		}
		lex_addch('\0', file);
		return LEX_COMMENT;
	}
	
	if (ch != '*') {
		lex_unget(ch, file);
		return '/';
	}
	
	lex_addch('/', file);
	
	/* hate this */
	do {
		lex_addch(ch, file);
		while ((ch = lex_getch(file)) != '*') {
			if (ch == EOF)
				return error(ERROR_LEX, "malformatted comment"," ");
			else
				lex_addch(ch, file);
		}
		lex_addch(ch, file);
	} while ((ch = lex_getch(file)) != '/');
	
	lex_addch('/',  file);
	lex_addch('\0', file);
	
	return LEX_COMMENT;
}

static int lex_getsource(struct lex_file *file) {
	int ch = lex_get(file);
	
	/* skip char/string/comment */
	switch (ch) {
		case '\'': return lex_skipchr(file);
		case '"':  return lex_skipstr(file);
		case '/':  return lex_skipcmt(file);
		default:   return ch;
	}
}

int lex_token(struct lex_file *file) {
	int ch = lex_getsource(file);
	int it;
	
	/* valid identifier */
	if (ch > 0 && (ch == '_' || isalpha(ch))) {
		lex_clear(file);
		while (ch > 0 && (isalpha(ch) || isdigit(ch) || ch == '_')) {
			lex_addch(ch, file);
			ch = lex_getsource(file);
		}
		lex_unget(ch,   file);
		lex_addch('\0', file);
		
		/* look inside the table for a keyword .. */
		for (it = 0; it < sizeof(lex_keywords)/sizeof(*lex_keywords); it++)
			if (!strncmp(file->lastok, lex_keywords[it], sizeof(lex_keywords[it])))
				return it;
				
		return LEX_IDENT;
	}
	return ch;
}

void lex_reset(struct lex_file *file) {
	file->current = 0;
	file->last    = 0;
	file->length  = file->size;
	fseek(file->file, 0, SEEK_SET);
	
	memset(file->peek,   0, sizeof(file->peek  ));
	memset(file->lastok, 0, sizeof(file->lastok));
}

int lex_debug(struct lex_file *file) {
	int list_do       = 0;
	int list_else     = 0;
	int list_if       = 0;
	int list_while    = 0;
	int list_break    = 0;
	int list_continue = 0;
	int list_return   = 0;
	int list_goto     = 0;
	int list_for      = 0;
	int token         = 0;
	printf("===========================\nTOKENS:   \n===========================\n");
	while ((token = lex_token(file)) != ERROR_LEX && file->length >= 0) {
		if (token != -1) {
			switch (token) {
				case 0: list_do      ++; break;
				case 1: list_else    ++; break;
				case 2: list_if      ++; break;
				case 3: list_while   ++; break;
				case 4: list_break   ++; break;
				case 5: list_continue++; break;
				case 6: list_return  ++; break;
				case 7: list_goto    ++; break;
				case 8: list_for     ++; break;
			}
		}
		if (token >= 33 && token <= 126)
			putchar(token);
	}
	printf("\n===========================\nBRANCHES \n===========================\n");
	printf("\t if       % 8d\n", list_if);
	printf("\t else     % 8d\n", list_else);
	printf("===========================\nLOOPS      \n===========================\n");
	printf("\t for      % 8d\n", list_for);
	printf("\t while    % 8d\n", list_while);
	printf("\t do       % 8d\n", list_do);
	printf("===========================\nSTATEMENTS \n===========================\n");
	printf("\t break    % 8d\n", list_break);
	printf("\t continue % 8d\n", list_continue);
	printf("\t return   % 8d\n", list_return);
	printf("\t goto     % 8d\n", list_goto);
	printf("===========================\nIDENTIFIERS\n===========================\n");
	lex_reset(file);
	while ((token = lex_token(file)) != ERROR_LEX && file->length >= 0)
		if (token == LEX_IDENT)
			printf("%s ", file->lastok);
	lex_reset(file);
	return 1;
}
