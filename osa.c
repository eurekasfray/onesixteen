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
// ...
//==============================================================================

#define ENUM int // type for anonymous enums
#define uint8 unsigned char 
#define uint16 unsigned short int

typedef uint8 byte;
typedef uint16 word;

//==============================================================================
// AST
//==============================================================================

// Line types

enum {
    LT_UNDEFINED,              // A line whose type is not yet defined
    LT_EMPTY,                  // An empty line
    LT_LABEL,                  // A line with only a label
    LT_LABEL_MNEMONIC,         // A line with a label and mnemonic
    LT_LABEL_MNEMONIC_OPERAND, // A line with a label, a mnemonic, and one or more operands
    LT_MNEMONIC,               // A line with only a mnemonic
    LT_MNEMONIC_OPERAND        // A line with a mnemonic, and one or more operands
};

// Node types

enum {
    NT_LINE,
    NT_TOKEN,
    NT_EOF
};

// Line node

struct line_node {
    ENUM node_type;
    ENUM line_type;
    unsigned int lineno;
};

// Token node

struct token_node {
    ENUM node_type;
    struct token *token;
    unsigned int colno;
};

// EOF node

struct eof_node {
    ENUM node_type;
};

//==============================================================================
// Node
//==============================================================================

// Node

struct node {
    void *data;
    struct node *sibling;
    struct node *subtree;
};

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

// Tree builder

struct node *get_ast();
struct node *ast_line(struct node *, ENUM, unsigned int);
struct node *ast_linesibl(struct node *, ENUM, unsigned int);
struct node *ast_token(struct node *, struct token *, unsigned int);
struct node *ast_tokensibl(struct node *, struct token *, unsigned int);
struct node *ast_eof(struct node *);
struct node *ast_eofsibl(struct node *);
void dump_ast(struct node *);

// Lexer

int get_next_char();
struct token *get_next_token();

// Lexer / Operations for token data structure

struct token *new_token();
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

// Node operations

struct node *new_node();

// Tree operations

struct node *new_tree();
struct node *add_sibling(struct node *);
struct node *add_subtree(struct node *);
bool tree_empty(struct node *);

// Error-trapped functions

FILE *efopen(const char *, const char *);
void *emalloc(size_t);

//==============================================================================
// Assembler components
//==============================================================================

FILE *src; // source file
int input; // stores character retrieved from source file
word lc;   // location counter

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
    // Init the location counter
    lc = 0;

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
            s = dupstr("string");
            break;
            
        case t_dquote:
            s = dupstr("string");
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
// Tree builder
//=============================================================================

// Build tree

struct node *get_ast()
{
    // The tree builder builds an internal representation of the source file
    // from a stream of tokens fed to it by the lexer. The source is represented
    // internally as a tree. Each line of the source becomes a node. And for
    // each token found on a line, the tree builder adds it to the tree as
    // a child of the line on which it was found.
    //  
    // The tree is organized into three levels: root, lines, and tokens.
    // Naturally, the root node lives on the first level. All lines live beneath
    // the root node, on the second level, as children of the root node. Lastly,
    // tokens live on the third level, as children of their belonging lines.
    //
    // The tree builder uses pointers to point to each level of the tree:
    // A pointer that points to the root node, one that points to the line nodes,
    // and lastly one that points to the token nodes. The root-node pointer is
    // mostly ignored by the tree builder. However, the pointers to the line
    // nodes and token nodes are used generally by the tree builder. The pointer
    // to the line node is used to point to the last line node accessed by the
    // tree builder. The same applies for the token-node pointer, where it is
    // used to point to the last token node accessed by the tree builder.
    // The purpose for their use is simple: The tree builder needs to know where
    // to add the next line node or next token node. So, these pointers always
    // point to the last added line and token, respectively.
    //
    // Handling first token. If EOF is the first token, create an EOF node on the
    // line level. Encountering an EOF as the first token means that the source
    // file is empty and contains no source code. If EOL is the first token,
    // create an LINE node on the line level. Encountering an EOL as the first
    // token means that first line of the source code contains no source code;
    // regardless, add it to the tree. However, if neither an EOF or EOL token
    // is encountered, add the token to a new line. Create a LINE node and
    // attach the token to it.
    //
    // Handling remaining tokens. All tokens are processed until an EOF token is
    // encountered, because it indicates that we have reached the end of the
    // source file. The stream of tokens from the lexer is consumed one
    // token at a time. Each token is checked and added to the tree
    // accordingly: If the token is an EOL token, then it is added as a
    // LINE. But for any other token, we simply append them as tokens to
    // the line. [Rewrite for coherency.]

    struct token *token; // stores retrieved token
    struct node *rootn;  // always points to the root node
    struct node *linen;  // points only to line nodes
    struct node *tokenn; // points only to token nodes
    
    rootn = new_tree();
    
    // Handle first token
    token = get_next_token();
    if (token->type == t_eof) {
        linen = ast_eof(rootn);
    }
    else if (token->type == t_eol) {
        linen = ast_line(rootn, LT_UNDEFINED, 0);
    }
    else {
        linen = ast_line(rootn, LT_UNDEFINED, 0);
        tokenn = ast_token(linen, token, 0);
    }
    
    // Handle remaining tokens
    token = get_next_token();
    while (token->type != t_eof) {
        if (token->type == t_eol) {
            // We encountered a new line... create a line node
            linen = ast_linesibl(linen, LT_UNDEFINED, 0);
        }
        else {
            // Does this line node have any token children? If not, create
            // a new token child. However if it already has a child or children,
            // create the new token as a sibling.
            if (linen->subtree == NULL) { 
                tokenn = ast_token(linen, token, 0);
            }
            else { 
                tokenn = ast_tokensibl(tokenn, token, 0);
            }
        }
        token = get_next_token();
    }
    linen = ast_eofsibl(linen);
    
    return rootn;
}

// Create new line node

struct node *ast_line(struct node *parent, ENUM line_type, unsigned int lineno)
{
    struct line_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct line_node));
    p->node_type = NT_LINE;
    p->line_type = line_type;
    p->lineno = lineno;
    n = add_subtree(parent);
    n->data = p;
    return n;
}

// Create new sibling line node

struct node *ast_linesibl(struct node *sister, ENUM line_type, unsigned int lineno)
{
    struct line_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct line_node));
    p->node_type = NT_LINE;
    p->line_type = line_type;
    p->lineno = lineno;
    n = add_sibling(sister);
    n->data = p;
    return n;
}

// Create new token node

struct node *ast_token(struct node *parent, struct token *token, unsigned int colno)
{
    struct token_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct token_node));
    p->node_type = NT_TOKEN;
    p->token = token;
    p->colno;
    n = add_subtree(parent);
    n->data = p;
    return n;
}

// Create new sibling token node

struct node *ast_tokensibl(struct node *sister, struct token *token, unsigned int colno)
{
    struct token_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct token_node));
    p->node_type = NT_TOKEN;
    p->token = token;
    p->colno;
    n = add_sibling(sister);
    n->data = p;
    return n;
}

// Create new EOF node

struct node *ast_eof(struct node *parent)
{
    struct eof_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct eof_node));
    p->node_type = NT_EOF;
    n = add_subtree(parent);
    n->data = p;
    return n;
}

// Create new EOF node as a sibling

struct node *ast_eofsibl(struct node *sister)
{
    struct eof_node *p;
    struct node *n;
    
    p = emalloc(sizeof(struct eof_node));
    p->node_type = NT_EOF;
    n = add_sibling(sister);
    n->data = p;
    return n;
}

// Dump print AST (debug only)

void dump_ast(struct node *root)
{
    // TODO: Rewrite this code elegantly

    struct node *lp; // current line node
    struct node *tp; // current token node
    struct line_node *ln;
    struct token_node *tn;
    int line; // line counter
    
    line = 1;
    lp = root->subtree; // get first line
    
    ln = lp->data;
    while (lp->sibling != NULL) {
        printf("Line %d\n\n", line);
        tp = lp->subtree; // get line subtree
        if (tp == NULL) {
            printf("  Empty\n\n");
        }
        else {
            tn = tp->data;
            while (tp->sibling != NULL) {
                printf("  Token\n");
                //printf("%sLexeme: %s\n", pad, tn->token->lexeme);
                printf("  Type: %s\n\n", get_meaning(tn->token->type));
                tp = tp->sibling;
                tn = tp->data;
            }
            printf("  Token\n");
            //printf("  Lexeme: %s\n", tn->token->lexeme);
            printf("  Type: %s\n\n", get_meaning(tn->token->type));
        }
        line++;
        lp = lp->sibling;
        //ln = lp->data;
    }
    printf("EOF\n\n");
}

//=============================================================================
// Lexer
//============================================================================

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
    
    enum {
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
    };
    
    ENUM current_state;  // current state
    ENUM next_state;     // next state
    bool done;           // used to indicate end of tokenization process
    struct token *token; // token

    token = new_token();
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

struct token *new_token()
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
    return eval(p,2);
}

// Evaluate octal

int eval_oct(const char *s)
{   
    char *p;
    p = dupstr(s);
    p[strlen(p)-1] = '\0';
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
        return eval(p,10);
    }
    else {
        return eval(p,10);
    }
}

// Evaluate hexadecimal

int eval_hex(const char *s)
{
    char *p;
    p = dupstr(s);
    p[strlen(p)-1] = '\0';
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
    // This assembler features simple string syntax. So, only remove the
    // quotation marks.

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

// Recognize any integer numeral

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
// Node operations
//=============================================================================

// Create new node

struct node *new_node()
{
    struct node *p;
    p = emalloc(sizeof(struct node));
    p->data = NULL;
    p->sibling = NULL;
    p->subtree = NULL;
    return p;
}

//=============================================================================
// Tree operations
//=============================================================================

// Create tree

struct node *new_tree()
{
    struct node *p;
    p = new_node();
    return p;
}

// Add sibling

struct node *add_sibling(struct node *target)
{
    // This operation adds a sibling to the specified target node.
    // The operation first checks for two things in order to determine
    // what action to take. It checks to see whether the target node
    // is terminal or not. If the target node is terminal, then it
    // simply appends the new sibling node to the target. However,
    // if the target already has a sibling attached to it, then the
    // operation must insert the new sibling between the target
    // and its existing sibling node.

    struct node *old;
    struct node *node;
    
    if (target->sibling == NULL) {
        node = new_node();
        target->sibling = node;
        return node;
    }
    else {
        old = target->sibling;
        node = new_node();
        target->sibling = node;
        node->sibling = old;
        return node;
    }
}

// Add subtree

struct node *add_subtree(struct node *target)
{
    // This operation adds a sibling to the specified target node
    
    struct node *old;
    struct node *node;

    if (target->subtree == NULL) {
        node = new_node();
        target->subtree = node;
        return node;
    }
    else {
        old = target->subtree;
        node = new_node();
        target->subtree = node;
        node->subtree = old;
        return node;
    }
}

// Is tree empty?

bool tree_empty(struct node *root)
{
    if (root->subtree == NULL) {
        return true;
    }
    return false;
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