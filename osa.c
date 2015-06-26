//==============================================================================/*
// Onesixteen assembler by eurekasfray
// https://github.com/eurekasfray/onesixteen
//==============================================================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

//==============================================================================
// Token
//==============================================================================

// Token Type

typedef enum token_type {
    t_id,
    t_int,
    t_colon,
    t_squote,
    t_dquote,
    t_eol,
    t_eof,
    t_unknown
} token_type;

// Token

struct token {
    char lexeme[256]; // string treated like stack stores lexeme captured from source
    int top;          // used by stack operations to point to top of lexeme
    bool eol;         // special flag used to indicate EOL
    bool eof;         // also a special flag for EOF
    token_type type;  // used to indicate token type
    int intval;       // stores evaluated integer value
    char *strval;     // ...
};

//==============================================================================
// Prototypes
//==============================================================================

// Error output

void error(const char *, ... );
void fail(const char *, ... );

// Misc

void display_usage(const char *);
void init();
char *get_meaning(token_type);

// Lexer

int get_next_char();
struct token *get_next_token();

// Lexer / Operations for token data structure

struct token *create_token();
void push_to_lexeme(struct token *, int);
int pop_from_lexeme(struct token *);
void flush_lexeme(struct token *);

// Lexer / Evaluators

int eval_bin(const char *);
int eval_oct(const char *);
int eval_dec(const char *);
int eval_hex(const char *);
int eval(const char *, int);
int get_value(int);
char *eval_sqstr(char *);
char *eval_dqstr(char *);

// Lexer / Recognizers 

bool is_terminal(const char *, const char *);
bool is_id(const char *);
bool is_int(const char *);
bool is_bin(const char *);
bool is_oct(const char *);
bool is_dec(const char *);
bool is_hex(const char *);
bool is_sqstr(const char *);
bool is_dqstr(const char *);

// Lexer / Atom recognizers

bool is_bindigit(int);
bool is_octdigit(int);
bool is_decdigit(int);
bool is_hexdigit(int);
bool is_digit(int);
bool is_letter(int);
bool is_visible_ascii_character(int);

// Lexer / Misc recognizers

bool is_eos(int);
bool is_eol(int);
bool is_eof(int);
bool is_binsym(int);
bool is_octsym(int);
bool is_decsym(int);
bool is_hexsym(int);
bool is_comment_initor(int);
bool is_underscore(int);
bool is_sqmark(int);
bool is_dqmark(int);
bool is_symbol(int);
bool is_whitespace(int);

// Misc helpers

int touppercase(int);
int tolowercase(int);
char *substr(const char *, size_t);
char *dupstr(const char *);

// Error-trapped functions

FILE *efopen(const char *, const char *);
void *emalloc(size_t);

//==============================================================================
// Assembler components
//==============================================================================

FILE *src; // source file
int input; // stores character retrieved from source file

//==============================================================================
// Error output
//==============================================================================

// Report error

void error(const char *format, ... )
{
    printf("Error: ");
    va_list args;
    va_start(args,format);
    vprintf(format,args);
    va_end(args);
    printf("\n");
}

// Report error and exit

void fail(const char *format, ... )
{
    char *s;
    s = emalloc(strlen(format)+1);
    va_list args;
    va_start(args,format);
    vsprintf(s,format,args);
    va_end(args);
    error("%s",s);
    exit(EXIT_FAILURE);
}

//==============================================================================
// Main
//==============================================================================

int main(int argc, char *argv[])
{
    if (argc != 2) {
        display_usage(argv[0]);
        return 0;
    }
    else {
        src = efopen(argv[1],"rb");
        init();
    }
    return 0;
}

//=============================================================================
// Display usage
//=============================================================================

// Display assembler usage

void display_usage(const char *self)
{
    printf("Usage: %s <file>\n", self);
}

//=============================================================================
// Initialization
//=============================================================================

// Initialize things

void init()
{
    // Get first character for the lexer to start with
    input = get_next_char();
}

//=============================================================================
// Human-readable token types
//=============================================================================

// Return English meaning of given token

char *get_meaning(token_type type)
{
    char *s;
    
    switch (type) {
        case t_id:
            s = dupstr("identifier");
            break;
            
        case t_int:
            s = dupstr("integer");
            break;
            
        case t_colon:
            s = dupstr("colon");
            break;
            
        case t_squote:
            s = dupstr("single quotation mark");
            break;
            
        case t_dquote:
            s = dupstr("double quotation mark");
            break;
            
        case t_eol:
            s = dupstr("end-of-line");
            break;
            
        case t_eof:
            s = dupstr("end-of-input");
            break;
            
        case t_unknown:
            s = dupstr("unknown");
            break;
    }
}

//=============================================================================
// Lexer
//=============================================================================

// Get next character from source file

int get_next_char()
{
    int c;
    
    c = fgetc(src);
    if (ferror(src)) {
        fail("Unable to read character from source file");
    }
    return c;    
}

// Get next token

struct token *get_next_token()
{
    // Tokenizer and Lexer

    /*
    -------------   -----------------   ----------  -----------------------------
    current state   input               next state  action
    -------------   -----------------   ----------  -----------------------------
    1               whitespace          1           ignore input; get next input
    1               symbol              2           do nothing
    1               eol                 3           do nothing    
    1               eof                 4           do nothing    
    1               sq.mark             5           do nothing
    1               dq.mark             6           do nothing
    1               comment initiator   7           do nothing
    1               anything else       8           do nothing
    2               ':'                 0           capture input; get next input
    3               eol                 0           capture input; get next input
    4               eof                 0           capture input; get next input
    5               sq.mark             5.1         capture input; get next input
    5.1             sq.mark             0           capture input; get next input
    5.1             eol                 0           do nothing
    5.1             eof                 0           do nothing
    5.1             comment initiator   0           do nothing
    5.1             anything else       5.1         capture input; get next input
    6               dq.mark             6.1         capture input; get next input
    6.1             dq.mark             0           capture input; get next input
    6.1             eol                 0           do nothing
    6.1             eof                 0           do nothing
    6.1             comment initiator   0           do nothing
    6.1             anything else       6.1         capture input; get next input
    7               eol                 1           do nothing
    7               eof                 1           do nothing
    7               anything else       7           ignore input; get next input
    8               whitespace          0           do nothing
    8               symbol              0           do nothing
    8               eol                 0           do nothing
    8               eof                 0           do nothing
    8               sq.mark             0           do nothing
    8               dq.mark             0           do nothing
    8               comment initiator   0           do nothing
    8               anything else       8           capture input; get next input
    0               lexeme              -           ...    
    -------------   -----------------   ----------  -----------------------------
    */
    
    typedef enum state {
        S0,     // State 0      ...
        S1,     // State 1      ...
        S2,     // State 2      ...
        S3,     // State 3      ...
        S4,     // State 4      ...
        S5,     // State 5      ...
        S5_1,   // State 5.1    ...
        S6,     // State 6      ...
        S6_1,   // State 6.1    ...
        S7,     // State 7      ...
        S8      // State 8      ...
    } state;
    
    state current_state; // current state
    state next_state;    // next state
    bool done;           // used to indicate end of tokenization process
    struct token *token; // token

    token = create_token();
    flush_lexeme(token);
    
    done = false;
    next_state = S1;
    
    while (!done) {
    
        current_state = next_state;
        
        switch(current_state) {

            // Tokenizer
            
            case S1:
                if (is_whitespace(input)) {
                    input = get_next_char();
                    next_state = current_state;
                }
                else if (is_symbol(input)) {
                    next_state = S2;
                }
                else if (is_eol(input)) {
                    next_state = S3;
                }
                else if (is_eof(input)) {
                    next_state = S4;
                }
                else if (is_sqmark(input)) {
                    next_state = S5;
                }
                else if (is_dqmark(input)) {
                    next_state = S6;
                }
                else if (is_comment_initor(input)) {
                    next_state = S7;
                }
                else {
                    next_state = S8;
                }
                break;
                
            case S2:
                if (input == ':') {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = S0;
                }
                break;
                
            case S3:
                if (is_eol(input)) {
                    token->eol = true;
                    input = get_next_char();
                    next_state = S0;
                }
                break;
                
            case S4:
                if (is_eof(input)) {
                    token->eof = true;
                    input = get_next_char();
                    next_state = S0;
                }
                break;
                
            case S5:
                if (is_sqmark(input)) {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = S5_1;
                }
                break;
                
            case S5_1:
                if (is_sqmark(input)) {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = S0;
                }
                else if (is_eol(input)) {
                    next_state = S0;
                }
                else if (is_eof(input)) {
                    next_state = S0;
                }
                else if (is_comment_initor(input)) {
                    next_state = S0;
                }
                else {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = current_state;
                }
                break;
                
            case S6:
                if (is_dqmark(input)) {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = S6_1;
                }
                break;
                
            case S6_1:
                if (is_dqmark(input)) {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = S0;
                }
                else if (is_eol(input)) {
                    next_state = S0;
                }
                else if (is_eof(input)) {
                    next_state = S0;
                }
                else if (is_comment_initor(input)) {
                    next_state = S0;
                }
                else {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = current_state;
                }
                break;
                
            case S7:
                if (is_eol(input)) {   
                    next_state = S1;
                }
                else if (is_eof(input)) {
                    next_state = S1;
                }
                else {
                    input = get_next_char();
                    next_state = current_state;
                }
                break;
                
            case S8:
                if (is_whitespace(input)) {
                    next_state = S0;
                }
                else if (is_symbol(input)) {
                    next_state = S0;
                }
                else if (is_eol(input)) {
                    next_state = S0;
                }
                else if (is_eof(input)) {
                    next_state = S0;
                }
                else if (is_sqmark(input)) {
                    next_state = S0;
                }
                else if (is_dqmark(input)) {
                    next_state = S0;
                }
                else if (is_comment_initor(input)) {
                    next_state = S0;
                }
                else {
                    push_to_lexeme(token, input);
                    input = get_next_char();
                    next_state = current_state;
                }
                break;

            // Lexer
                
            case S0:
            
                done = true;
                
                if (token->eol) {
                    token->type = t_eol;
                }
                else if (token->eof) {
                    token->type = t_eof;
                }
                else if (is_terminal(":",token->lexeme)) {
                    token->type = t_colon;
                }
                else if (is_bin(token->lexeme)) {
                    token->type = t_int;
                    token->intval = eval_bin(token->lexeme);
                }
                else if (is_oct(token->lexeme)) {
                    token->type = t_int;
                    token->intval = eval_oct(token->lexeme);
                }
                else if (is_dec(token->lexeme)) {
                    token->type = t_int;
                    token->intval = eval_dec(token->lexeme);
                }
                else if (is_hex(token->lexeme)) {
                    token->type = t_int;
                    token->intval = eval_hex(token->lexeme);
                }
                else if (is_id(token->lexeme)) {
                    token->type = t_id;
                }
                else if (is_sqstr(token->lexeme)) {
                    token->type = t_squote;
                    token->strval = eval_sqstr(token->lexeme);
                }
                else if (is_dqstr(token->lexeme)) {
                    token->type = t_dquote;
                    token->strval = eval_dqstr(token->lexeme);
                }
                else {
                    token->type = t_unknown;
                }
                break;
        }
    }
    return token;
}

// TOKEN OPERATIONS

// Create new token

struct token *create_token()
{
    struct token *p;
    p = emalloc(sizeof(struct token));
    p->eol = false;
    p->eof = false;
    return p;
}

// Push character to lexeme

void push_to_lexeme(struct token *p, int c)
{
    if (p->top == 256) {
        fail("Something went wrong. Overflow occurred on lexeme stack");
    }
    
    p->lexeme[p->top++] = c;
    p->lexeme[p->top] = '\0';
}

// Pop character from lexeme

int pop_from_lexeme(struct token *p)
{
    int c;
    
    if (p->top == -1) {
        fail("Something went wrong. Underflow occurred on lexeme stack");
    }
    
    p->top--;
    c = p->lexeme[p->top];
    p->lexeme[p->top] = '\0';
    
    return c;
}

// Flush lexeme

void flush_lexeme(struct token *p)
{
    p->top = 0;
    p->lexeme[p->top] = '\0';
}

// EVALUATORS

// Evaluate binary

int eval_bin(const char *s)
{
    // Remove appended symbol and evaluate a binary number
    char *p;
    p = dupstr(s);
    p[strlen(p)-1] = '\0';
    printf(" %s ",p);
    return eval(p,2);
}

// Evaluate octal

int eval_oct(const char *s)
{   
    char *p;
    p = dupstr(s);
    p[strlen(p)-1] = '\0';
    printf(" %s ",p);
    return eval(p,8);
}

// Evaluate decimal

int eval_dec(const char *s)
{
    // Evaluate a decimal. Unlike other number systems, decimals are valid
    // with or without the appended symbol. Check for the symbol and remove
    // it if necessary before evaluating the decimal.

    char *p;

    p = dupstr(s);
    if (tolowercase(s[strlen(s)-1]) == 'd') {
        p[strlen(p)-1] = '\0';
        printf(" %s ",p);
        return eval(p,10);
    }
    else {
        printf(" %s ",p);
        return eval(p,10);
    }
}

// Evaluate hexadecimal

int eval_hex(const char *s)
{
    char *p;
    p = dupstr(s);
    p[strlen(p)-1] = '\0';
    printf(" %s ",p);
    return eval(p,16);
}

// Evaluator

int eval(const char *s, int base)
{
    int i;       // loop counter
    int val;     // stores integer value of digit
    int integer; // stores converted integer
    int place;   // stores place value
    
    integer = 0; // zero it
    place = 1;   // start at place value 1
    
    // Convert the given string to an integer value. Starting from end of
    // string and working our way to the beginning, get the integer value
    // for each digit and calculate its corresponding place value.
    // Sum the calculated place value for each digit.
        
    for(i=strlen(s)-1; i>=0; i--) {
        val = get_value(s[i]);          // get the value of the current digit
        integer += val * place;         // put digit in specified place value
        place *= base;                  // go to next place value
    }
    return integer;
}

// Lookup digit value

int get_value(int c)
{
    // Get the digit value of any digit. To do this, we use a lookup table,
    // a simple character array. This array is a map. Each digit maps to its
    // corresponding integer value.

    int i; // indexer
    char d[] = "0123456789abcdef"; // table map
    
    // Search table for digit and return its corresponding integer value
    for (i=0; i<strlen(d); i++) {
        if (tolowercase(c) == d[i]) {
            return i;
        }
    }

    // Return an indicator if no match
    return -1;
}

// Evaluate single-quote string

char *eval_sqstr(char *s)
{
    // This assembler features simple string syntax. So, only remove the quotation marks.

    char *p;
    p = substr(s+1,strlen(s)-2);
    return p;
}

// Evaluate double-quote string

char *eval_dqstr(char *s)
{
    return eval_sqstr(s);
}

// TERMINAL RECOGNIZER

// Match token to terminal

bool is_terminal(const char *terminal, const char *token)
{
    if (strcmp(terminal,token) == 0) {
        return true;
    }
    return false;
}

// LOW-LEVEL RECOGNIZERS

// Recognize identifier

bool is_id(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
                
                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept either a letter or an underscore; deny anything else.
                
                if (is_letter(c) || is_underscore(c)) {
                    next_state = 3;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept zero or more letters, digits or underscore characters;
                // deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else if (is_letter(c) || is_digit(c) || is_underscore(c)) {
                    next_state = current_state;
                }
                else {
                    next_state = 0;
                }
                break; 
        }
    }
    return validity;
}

// Recognize any integer representation

bool is_int(const char *s)
{
    if (is_bin(s) || is_oct(s) || is_dec(s) || is_hex(s)) { 
        return true;
    }
    return false;
}

// Recognize binary numeral

bool is_bin(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
                
                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept one binary digit. Deny anything else
                
                if (is_bindigit(c)) {
                    next_state = 3;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept zero or more binary digits. Deny anything else.
                
                if (is_bindigit(c)) {
                    next_state = current_state;
                }
                else if (is_binsym(c)) {
                    next_state = 4;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else {
                    next_state = 0;
                }
                break;
        }
    }
    
    return validity;
}

// Recognize octal numeral

bool is_oct(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) {
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
                
                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept one octal digit; deny anything else.
                
                if (is_octdigit(c)) {
                    next_state = 3;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept zero or more octal digits; deny anything else.
                
                if (is_octdigit(c)) {
                    next_state = current_state;
                }
                else if (is_octsym(c)) {
                    next_state = 4;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else {
                    next_state = 0;
                }
                break;
        }
    }
    return validity;
}

// Recognize decimal numeral

bool is_dec(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
            
                validity = true;
                done = true;
                break;
                
            case 2:
                
                // Accept one decimal digit; deny anything else.
            
                if (is_decdigit(c)) {
                    next_state = 3;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept zero or more decimal digits; deny anything else.
            
                if (is_decdigit(c)) {
                    next_state = current_state;
                }
                else if (is_decsym(c)) {
                    next_state = 4;
                }
                else if (is_eos(c)) {
                    next_state = 1;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else {
                    next_state = 0;
                }
                break;
        }
    }
    return validity;
}

// Recognize hexadecimal numeral

bool is_hex(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
                
                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept one hex digit; deny anything else.
                
                if (is_hexdigit(c)) {
                    next_state = 3;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept zero or more hex digits; or accept hexadecimal symbol.
                // Deny anything else.
                
                if (is_hexdigit(c)) {
                    next_state = current_state;
                }
                else if (is_hexsym(c)) {
                    next_state = 4;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else {
                    next_state = 0;
                }
                break;
        }
    }
    return validity;
}

// Recognize single-quote string

bool is_sqstr(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid
                
                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid
                
                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept opening single-quotation mark. Deny anything else.
                
                if (is_sqmark(c)) {
                    next_state = 3;
                }
                else {  
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept any visible ASCII character, except for the
                // single-quotation mark. Or, accept closing single-quotation mark.
                // Deny anything else.
                
                if (is_visible_ascii_character(c) && c != '\'') {
                    next_state = current_state;
                }
                else if (is_sqmark(c)) {
                    next_state = 4;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.
                
                if (is_eos(c)) {
                    next_state = 1;
                }
                else {  
                    next_state = 0;
                }
        }
    }
    return validity;
}

// Recognize double-quote string

bool is_dqstr(const char *s)
{
    int current_state;
    int next_state;
    bool validity;
    bool done;
    int c;
    int i;
    
    i = 0;
    done = false;
    next_state = 2;
    
    while (!done) { 
    
        c = s[i++];
        current_state = next_state;
        
        switch(current_state) {
        
            case 0:
            
                // Invalid

                validity = false;
                done = true;
                break;
                
            case 1:
            
                // Valid

                validity = true;
                done = true;
                break;
                
            case 2:
            
                // Accept opening double-quotation mark. Deny all others.
               
                if (is_dqmark(c)) {
                    next_state = 3;
                }
                else {  
                    next_state = 0;
                }
                break;
                
            case 3:
            
                // Accept any visible ASCII character except the double-quotation
                // mark. Or, accept closing double-quotation mark.
                // Deny anything else.

                if (is_visible_ascii_character(c) && c != '"') {
                    next_state = current_state;
                }
                else if (is_dqmark(c)) {
                    next_state = 4;
                }
                else {
                    next_state = 0;
                }
                break;
                
            case 4:
            
                // Accept EOS; deny anything else.

                if (is_eos(c)) {
                    next_state = 1;
                }
                else {  
                    next_state = 0;
                }
        }
    }
    return validity;
}

// ATOM RECOGNIZERS 

// Recognize binary digit

bool is_bindigit(int c)
{
    if (c == '0' || c == '1') {
        return true;
    }
    return false;
}

// Recognize octal digit

bool is_octdigit(int c)
{
    if (is_digit(c) && (c!='8' && c!='9')) {
        return true;
    }
    return false;
}

// Recognize decimal digit

bool is_decdigit(int c)
{
    if (is_digit(c)) {
        return true;
    }
    return false;
}

// Recognize hexadecimal digit

bool is_hexdigit(int c)
{
    if (is_digit(c)) {
        return true;
    }
    else if (tolowercase(c) >= 'a' && tolowercase(c) <= 'f') {
        return true;
    }
    return false;    
}

// Recognize digit

bool is_digit(int c)
{
    if (c >= '0' && c <= '9') {
        return true;
    }
    return false;
}

// Recognize letter

bool is_letter(int c)
{
    c = tolowercase(c);
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    return false;
}

// Recognize any visible ASCII character

bool is_visible_ascii_character(int c) 
{
    // Visible ASCII characters run a value range of 32 - 126
    if (c >= 32 && c <= 126) { 
        return true;
    }
    return false;
}

// MISC RECOGNIZERS

// Recognize end-of-string character

bool is_eos(int c)
{
    if (c == '\0') {
        return true;
    }
    return false;
}

// Recognize end-of-line character

bool is_eol(int c)
{
    if (c == '\n') {
        return true;
    }
    return false;
}

// Recognize end-of-file indicator

bool is_eof(int c)
{
    if (c == EOF) {
        return true;
    }
    return false;
}

// Recognize binary notation symbol

bool is_binsym(int c)
{
    if (tolowercase(c) == 'b') {
        return true;
    }
    return false;
}

// Recognize octal notation symbol

bool is_octsym(int c)
{
    if (tolowercase(c) == 'o') {
        return true;
    }
    return false;
}

// Recognize decimal notation symbol

bool is_decsym(int c)
{
    if (tolowercase(c) == 'd') {
        return true;
    }
    return false;
}

// Recognize hexadecimal notation symbol

bool is_hexsym(int c)
{
    if (tolowercase(c) == 'h') {
        return true;
    }
    return false;
}

// Recognize comment initiator

bool is_comment_initor(int c)
{
    if (c == ';') {
        return true;
    }
    return false;
}

// Recognize underscore character

bool is_underscore(int c)
{
    if (c == '_') {
        return true;
    }
    return false;
}

// Recognize single quotation mark

bool is_sqmark(int c)
{
    if (c == '\'') {
        return true;
    }
    return false;
}

// Recognize double quotation mark

bool is_dqmark(int c)
{
    if (c == '"') {
        return true;
    }
    return false;
}

// Recognize a symbol

bool is_symbol(int c)
{
    switch (c) {
        case ':':
        return true;
        
        default:
        return false;
    }
}

// Recognize whitespace

bool is_whitespace(int c)
{
    switch (c) {
        case '\t': // horizontal tab
        case '\v': // vertical tab
        case '\r': // carriage return
        case ' ':  // space
        return true;
        
        default:
        return false;
    }
}

// HELPERS

// Convert a lowercase to uppercase

int touppercase(int c)
{
    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }
    return c;
}

// Convert an uppercase to lowercase

int tolowercase(int c)
{
    if (c >= 'A' && c <= 'Z') {
        c += 32;
    }
    return c;
}

// Get substring

char *substr(const char *s, size_t len)
{
    char *p;
    if (len == 0) {
        p = emalloc(1);
        strcpy(p,"");
    }
    else {
        p = emalloc(len+1);
        memcpy(p,s,len);
        p[len] = '\0'; // add string terminator
    }
    return p;
}

// Duplicate string

char *dupstr(const char *s)
{
    char *p;
    p = emalloc(strlen(s)+1);
    strcpy(p,s);
    return p;
}

//=============================================================================
// Error-trapped functions
//=============================================================================

// fopen()

FILE *efopen(const char *filename, const char *mode)
{
    FILE *p;
    p = fopen(filename,mode);
    if (!p) {
        fail("Something went wrong. Unable to open file %s", filename);
    }
    return p;
}

// malloc()

void *emalloc(size_t size)
{
    void *p;
    p = malloc(size);
    if (!p) {
        fail("Something went wrong. Unable to allocate memory");
    }
    return p;
}
