#include<list>
#include<map>
#include<set>
#include "def.h"

/* Define Result */
typedef enum Vtype { 
    V_BOL, 
    V_STR, 
    V_INT, 
    V_BRE, 
    V_CON, 
    V_RET,
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
    std::set<Node*> *defs;
    ~STentry();
} STentry; 

/* Define function table entry */
typedef struct FTentry {
    int pnum;
    std::list<char*>  *plist; 
    Node *block;
    ~FTentry();
} FTentry;

/* define symbol table type */
typedef std::map<std::string, STentry*> STable;

// Traverse and interpret AST
Value* exec(Node* current, Node* parent, Node* ctrl, STable &loc_stable);

// define name of function return value in symbol table
#define RET_ADDR "-"
