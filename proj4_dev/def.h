/* Define expression type */
typedef enum Etype {
    E_OR, // || 
    E_AND, // && 
    E_EQ, // ==  
    E_NE, // != 
    E_LT, // <
    E_LE, // <= 
    E_GT, // > 
    E_GE, // >=
    E_MUL, // *
    E_DIV, // /
    E_ADD, // +
    E_SUB, // -
    E_NOT // !
} Etype;

/* Define AST node */
// Node type
typedef enum Ntype {
    N_BL, // Block
    N_WR, // write statement
    N_AS, // assignment statement
    N_DE, // declare w/o init
    N_SDE, // scalar declaration
    N_ODE, // declare object
    N_ADE, // declare array
    N_IF, // if statement
    N_WD, // while-do statement
    N_DW, // do-while statement
    N_BR, // break
    N_CO, // continue
    N_EX, // expression
    N_VA, // values: boolean, int, str
    N_ID, // identifier
    N_AR, // array access
    N_FA, // field access
    N_IFD, // initalize field;
    /* project 4 */
    N_PR, // program
    N_FD, // function definition
    N_FC, // function call
    N_RE, // return statement
    N_AT // assert statement
} Ntype;

typedef struct Node {
    Ntype type; // node type
    struct Node *left; // left child
    struct Node *right; // right child
    struct Node *ctrl; // control dependencies
    void* uses; // uses set 
    void* reach_defs; 

    union {
        Etype etype; // expression type
        void* ptr;
    } spec; // special field
    int lineno; // line number
    int err; // error bit

} Node;

/* Define YYSTYPE */ 
typedef union {
    char* str;
    int num; 
    Node *node;
    void* spec;
} YYSTYPE;
