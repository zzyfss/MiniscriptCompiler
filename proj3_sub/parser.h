/* Define Result */
typedef enum Vtype { V_BOL, V_STR, V_INT, 
V_BRE, V_CON, 
V_UND
} Vtype;

/* Define Value as return value from traversing an AST */
typedef struct Value
{
    union
    {
        int v_int;
        char* v_str;
    } val; // Actual values (boolean, int, str)
    Vtype type;
    ~Value();
} Value;

// Traverse and interpret AST
Value* exec(Node* current, Node* parent);

/* Define symbol table entry */
typedef enum Stype { 
    S_OBJ, // object
    S_ARR, // array
    S_SCA, // scalar
    S_UNK // unknow type
} Stype;

typedef struct STentry {
    Stype type;
    Value* value;
    ~STentry();
} STentry; 

