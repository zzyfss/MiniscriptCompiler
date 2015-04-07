#include "def.h"
#include "parser.h"
#include<list>
#include<iostream>
#include<map>
#include<string.h>
#include<string>

using namespace std;

void print(Value *v);

int getBool(Value *v);

void clear(string name);

void valcpy(Value* dest, const Value* src);

void error(int lineno, const char*  msg, int* err_flag){
    if(! *err_flag){
        cerr <<  "Line " << lineno << ", " << msg << endl;
        *err_flag = 1; 
    }
}



// Symbol table
map<string, STentry*> stable;

/* Interpret AST node */
Value* exec(Node* current, Node* parent_stmt){
    if(current == NULL){
        return NULL;
    }
    switch(current->type){
        case N_BL: 
        {
            /* Block node */
            list<Node*> *slist = (list<Node*> *) current->spec.ptr;
            
            // iterate statement list
            for(auto stmt: *slist){
                Value* v = exec(stmt, parent_stmt);
                if( v != NULL && (v->type == V_BRE || v->type == V_CON)){
                    // skip the rest of statements
                    return v;
                }
                delete v;
            }
            
            return NULL;
        }
        case N_WR:
        {
            /* Write statement */
            list<Node*> *plist = (list<Node*> *) current->spec.ptr;
            
            if(plist == NULL){
                // no arguments for write()
                return NULL;
            }
            for(auto para: *plist){
                /* Each parameter is considered a statement 
                 * for error reporting 
                 */
                Value* v = exec(para, para);
                print(v);
                delete v;
            }
            return NULL;
        }
        case N_IF:
        {
            /* if statement */
            Node* cond = (Node*) current->spec.ptr;
            Value* result = exec(cond, current);
            if(result->type == V_UND){
                error(cond->lineno, "condition unknown",&(current->err));
                // skips entire if statement
                delete result;
                return NULL;
            }
            else if(getBool(result)){
                delete result;
                return exec(current->left, NULL);
            }
            else{
                delete result;
                return exec(current->right, NULL);
            }
        } // end N_IF
        case N_WD:
        {
            /* while statement */
            Node* cond = current->left;
            Value* result = NULL;

            while(1){
                delete result;
                // evaluate condition
                result=exec(cond, current);
                if(result->type == V_UND){
                    error(cond->lineno, "condition unknown",&(current->err));
                    // skips entire while  statement
                    break;         
                }
                if( getBool(result) == 0){
                    // condition is false
                    break;
                }
                // execute body
                Value* value = exec(current->right, NULL);
                if(value != NULL && value->type == V_BRE){
                    // Break
                    delete value;
                    break;
                }
            }

            delete result;
            return NULL;
        }
        case N_DW:
        {
            /* do-while statement */
            Node* cond = current->left;
            Value* result = NULL;
            
            while(1){
                // execute body first
                Value* value = exec(current->right, NULL);
                if(value != NULL && value->type == V_BRE){
                    // Break
                    delete value;
                    break;
                }
                
                delete result;

                // evaluate condition
                result=exec(cond, current);
                if(result->type == V_UND){
                    error(cond->lineno, "condition unknown",&(current->err));
                    // skips entire while  statement
                    break;            
                }
                if( getBool(result) == 0){
                    // condition is false
                    break;
                }
            }

            delete result;
            return NULL;

        } // end N_DW
        case N_BR:{
            /* break */
            Value* val = new Value;
            val->type = V_BRE;
            return val;
        }
        case N_CO:{
            /* continue */
            Value* val = new Value;
            val->type = V_CON;
            return val;
        }
        case N_DE:{
            string vname = string((char*) current->spec.ptr);
            
            // clear old fields (array members) if any
            clear(vname);
            /* Declare without initialization */
            STentry* entry = new STentry;
            entry->type = S_UNK;

            stable[vname] = entry;
            return NULL;
        }
        case N_SDE:{
            /* Scalar declaration */
                
            string vname = string((char*) current->spec.ptr);
            
            // clear old fields (array members) if any
            clear(vname);
            
            STentry* entry = new STentry;
            entry->type = S_SCA;
            entry->value = exec(current->right, current);
        
            stable[vname] = entry;
            return NULL;
        } 
        case N_ODE:{
            /* Object declaration */

            string oname = string((char*) current->spec.ptr);
            // clear old fields (array members) if any
            clear(oname);
            STentry* entry = new STentry;
            entry->type = S_OBJ;
            
            // insert new entry
            stable[oname] = entry;

            // field initialization list
            list<Node*> *flist = (list<Node*>*) current->right;
            if(flist != NULL){
                for(auto ifd : *flist){
                    exec(ifd, current);
                }
            }
            return NULL;
        }
        case N_IFD:{
            /* Field initialization */
            // field name
            string fname = string((char*)current->spec.ptr);
            // obj name
            string oname = string((char*) parent_stmt->spec.ptr);

            // entry name
            string ename = oname + "." + fname;

            STentry *entry = new STentry;
            entry->type = S_SCA;
            entry->value = exec(current->right, parent_stmt);

            // insert new entry to symbol table
            stable[ename] = entry;

            return NULL;
        }
        case N_ADE:{
            string aname = string((char*)current->spec.ptr);
            // clear old variable
            clear(aname);
        
            /* Array declaration */
            STentry* aentry = new STentry;
            aentry->type = S_ARR;

            // insert new entry
            stable[aname] = aentry;
        
            // array size
            int asize = 0;

            list<Node*>* alist = (list<Node*>*) current->right;
            
            if(alist != NULL){
                // member init
                for(auto exp: *alist){
                    Value* avalue = exec(exp, current);
                    // member name = aname[index]
                    string mname = aname + '[' + to_string(asize) + ']';
                    // create an entry
                    STentry* mentry = new STentry;
                    mentry->type = S_SCA;
                    mentry->value = avalue;

                    // insert entry
                    stable[mname] = mentry;

                    // increment size
                    asize++;
                }
            }

            // set array size
            Value* size = new Value;
            size->val.v_int = asize;

            aentry->value = size;

            return NULL;
        } // end N_ADE
        case N_AS:{
            /* Assign statement */
            // Evaluate RHS first
            Value* value = exec(current->right, current);
            
            Node* left = current->left;

            string var_name;
            if(left->type == N_ID){
                var_name = string((char*)left->spec.ptr);
            }
            else if(left->type == N_FD){
                // object name
                string oname = string((char*)left->left);
                // field name
                string fname = string((char*)left->right);
                var_name = oname + '.' + fname;

                // check if obj exists
                STentry* oentry = stable[oname];
                
                if(oentry == NULL){ 
                    error(current->lineno, (oname + " undeclared").c_str(),
                        &(current->err));
                    oentry = new STentry;
                    oentry->type = S_OBJ;
                    // insert into table
                    stable[oname] = oentry;
                } else if (oentry->type != S_OBJ){
                    error(current->lineno, "type violation", &(current->err));
                    return NULL;
                }
            } // end type == N_FD
            else if(left->type == N_AR){
                // array name
                string aname = string((char*)left->spec.ptr);
                // index
                Value* idx = exec(left->left, current);
                if(idx->type != V_INT){
                    error(current->lineno, "type violation", &(current->err));
                    delete idx;
                    return NULL;
                }
                else{
                    // array member full name
                    var_name = aname + '[' + to_string(idx->val.v_int) + ']';
                    
                    STentry *aentry = stable[aname];
                    
                    if(aentry == NULL){
                        // array doesn't exist
                        error(current->lineno, 
                        (aname + " undeclared").c_str(),
                        &(current->err));
                        
                        // add new array entry
                        aentry = new STentry;
                        aentry->type = S_ARR;
                        
                        stable[aname] = aentry;
                        /* Need size? nah */
                        Value* size = new Value;
                        size->type = V_INT;
                        size->val.v_int = idx->val.v_int + 1;
                    } else if(aentry->type != S_ARR){
                        error(current->lineno, "type violation", &(current->err));
                        delete idx;
                        return NULL;
                    }
                }
            } // left->type == N_AR

            STentry* entry = stable[var_name];            
            if(entry == NULL){
                
                // Variable has not been declared!
                if(left->type == N_ID){
                    error(current->lineno, 
                        (var_name + " undeclared").c_str(), 
                        &(current->err));
                }

                // Add a new entry
                entry = new STentry;
                entry->type = S_SCA;
                entry->value = value;
                stable[var_name] = entry;
            }
            else if (entry->type == S_SCA){
                entry->value = value;
            }
            else if (entry->type == S_UNK){
                entry->value = value;
                entry->type = S_SCA;
            }
            else{
                // cannot assigne values to object/array
                error(current->lineno, "type violation", &(current->err));
            }
            return NULL;
        }
        case N_EX:
        {
            Value* lop = exec(current->left, parent_stmt);
            Value* rop = exec(current->right, parent_stmt);
            
            Value* result = new Value;
            
            int err_flag = parent_stmt->err;

            // Adjust lineno
            int lineno;
            if( current->right != NULL){
                lineno = current->right->lineno;
            }
            else if(current->left != NULL){
                lineno= current->left->lineno;
            }
            else{
                lineno = current->lineno;
            }

            // Handle undefined operands
            if(lop->type == V_UND || 
                rop != NULL && rop->type == V_UND) {
                result->type = V_UND;
                return result;
            }

            // Evaluate the expression
            switch(current->spec.etype){
                /* relational operations 
                 * (>, >=, <, <=, ==, !=)
                 */
                case E_GT: {
                     if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        result->val.v_int = (lop->val.v_int > rop->val.v_int);
                    }
                    break;
                }
                case E_GE:{
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        result->val.v_int = (lop->val.v_int >= rop->val.v_int);
                    }
                    break;
                }
                case E_LT: {
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        result->val.v_int = (lop->val.v_int < rop->val.v_int);
                    }
                    break;
                }
                case E_LE: {
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        result->val.v_int = (lop->val.v_int <= rop->val.v_int);
                    }
                    break;
                }
                case E_EQ: {
                    if(lop->type != rop->type){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else if(lop->type == V_INT){
                        result->type = V_BOL;
                        result->val.v_int = lop->val.v_int == rop->val.v_int;
                    }
                    else if(lop->type == V_STR){
                        result->type = V_BOL;
                        result->val.v_int = 0 == strcmp(lop->val.v_str, 
                                                    rop->val.v_str);
                    }
                    else if(lop->type == V_BOL){
                        result->type = V_BOL;
                        result->val.v_int = 0;
                        
                        if(lop->val.v_int!=0 && rop->val.v_int != 0){
                            // both are true
                            result->val.v_int = 1;
                        }
                        else if(lop->val.v_int == 0 && rop->val.v_int == 0){
                            // both are false
                            result->val.v_int = 1;
                        }
                    }
                    else{
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    break;
                } // end E_EQ
                case E_NE: {
                    if(lop->type != rop->type){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else if(lop->type == V_INT){
                        result->type = V_BOL;
                        result->val.v_int = lop->val.v_int != rop->val.v_int;
                    }
                    else if(lop->type == V_STR){
                        result->type = V_BOL;
                        result->val.v_int = 0 != strcmp(lop->val.v_str, 
                                                    rop->val.v_str);
                    }
                    else if(lop->type == V_BOL){
                        result->type = V_BOL;
                        result->val.v_int = 1;
                        
                        if(lop->val.v_int!=0 && rop->val.v_int != 0){
                            // both are true
                            result->val.v_int = 0;
                        }
                        else if(lop->val.v_int == 0 && rop->val.v_int == 0){
                            // both are false
                            result->val.v_int = 0;
                        }
                    }
                    else{
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    break;
                } // end E_NE

                /* boolean operations (||, &&, !) */
                case E_OR:{
                    // operands should be defined
                    if(lop->type == V_UND || rop->type == V_UND){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        
                        if(lop->type == V_STR){
                            // cast string to boolean
                            lop->type = V_BOL;
                            lop->val.v_int = strlen(lop->val.v_str) != 0;
                        }
                        if(rop->type == V_STR){
                            // cast string to boolean
                            rop->type = V_BOL;
                            rop->val.v_int = strlen(rop->val.v_str) != 0;
                        }
                        
                        // Evaluate result
                        if(lop->val.v_int ==0 && rop->val.v_int == 0){
                            // both are false
                            result->val.v_int = 0;
                        }
                        else{
                            result->val.v_int = 1;
                        }

                    }
                    break;
                } // end E_OR
                case E_AND:{
                    // operands should be defined
                    if(lop->type == V_UND || rop->type == V_UND){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_BOL;
                        
                        if(lop->type == V_STR){
                            // cast string to boolean
                            lop->type = V_BOL;
                            lop->val.v_int = strlen(lop->val.v_str) != 0;
                        }
                        if(rop->type == V_STR){
                            // cast string to boolean
                            rop->type = V_BOL;
                            rop->val.v_int = strlen(rop->val.v_str) != 0;
                        }
                        
                        // Evaluate result
                        if(lop->val.v_int ==0 ||  rop->val.v_int == 0){
                            // either is false
                            result->val.v_int = 0;
                        }
                        else{
                            result->val.v_int = 1;
                        }

                    }
                    break;                } // end E_AND
                case E_NOT:{
                    if(lop->type == V_UND){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else if(lop->type == V_INT || lop->type == V_BOL){
                        result->type = V_BOL;
                        result->val.v_int = !lop->val.v_int;
                    }
                    else if(lop->type == V_STR){
                        result->type = V_BOL;
                        result->val.v_int = (strlen(lop->val.v_str)==0);
                    }
                    break;
                } // end E_NOT
                
                /* arithmetic operations (*, /, +, -) */
                case E_MUL:{
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_INT;
                        result->val.v_int = lop->val.v_int * rop->val.v_int;
                    }
                    break;
                }
                case E_DIV:{
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;  
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        // Check divided-by-zero
                        if(rop->val.v_int == 0) {
                            error(current->lineno, "divided by zero", &err_flag);
                            result->type = V_UND;
                        }
                        else{
                            result->type = V_INT;
                            result->val.v_int = lop->val.v_int / rop->val.v_int;
                        }
                    }
                    break;
                }
                case E_ADD:{
                    if(lop->type != rop->type){
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else if(lop->type == V_INT){
                        // integer addition
                        result->type = V_INT;
                        result->val.v_int = lop->val.v_int + rop->val.v_int;
                    }
                    else if(lop->type == V_STR){
                        // string concatenation
                        
                        // check if operands contain <br />
                        if(strcmp(lop->val.v_str, "<br />") == 0 ||
                            strcmp(rop->val.v_str, "<br />") == 0){
                            
                            result->type = V_UND; 
                            error(lineno, "type violation", &err_flag);
                        }
                        else{
                            // concatenate two operands
                            int str_len = strlen(lop->val.v_str) +
                                    strlen(rop->val.v_str);

                            char buff[str_len+1];
                            strcpy(buff, lop->val.v_str);
                            strcat(buff, rop->val.v_str);
                            result->val.v_str = strdup(buff);
                            result->type = V_STR;
                        }
                    } // end else if V_STR
                    else{
                        // Addition doesn't support other types
                        result->type = V_UND;
                        error(lineno, "invalid operands", &err_flag);
                    }

                    break;
                } // end E_ADD
                case E_SUB:{
                    if(lop->type != V_INT || rop->type != V_INT){
                        // type violation
                        result->type = V_UND;
                        error(lineno, "type violation", &err_flag);
                    }
                    else{
                        result->type = V_INT;
                        result->val.v_int = lop->val.v_int - rop->val.v_int;
                    }
                    break;

                }
            } // end switch etype

            // delete immediate values
            delete lop;
            delete rop;
            // update the error bit
            parent_stmt->err = err_flag;

            return result;
        } // end N_EX
        case N_VA:
        {
            /* Value node */
            /* copy value */ 
            Value* src = (Value*) current->spec.ptr;
            Value* result = new Value;
            
            valcpy(result, src);
            
            return result;
        }
        case N_ID:
        {
            string vname = string((char*) current->spec.ptr);
            int lineno = current->lineno;
            int err_flag = parent_stmt->err;
            
            Value* result = new Value;

            // Retreive associated entry in symbol table
            STentry* entry = stable[vname];
            
            if(entry == NULL){
                // variable has not been declared (value error)
                error(lineno, (vname + " has no value").c_str(), &err_flag);
                
                result->type = V_UND;
            }
            else if(entry->type == S_UNK){
                // variable has been declared but not written (value error)
                error(lineno, (vname + " has no value").c_str(), &err_flag);
                result->type = V_UND;

            }else if(entry->type == S_OBJ){
                // object can not be a variable
                error(lineno, "type violation", &err_flag);

                result->type = V_UND;
            }
            else if(entry->type == S_ARR){
                error(lineno, "type violation", &err_flag);

                result->type = V_UND;
        
            }
            else {
                // copy value 
                valcpy(result, entry->value);
            }
            
            // update error flag
            parent_stmt->err = err_flag;
            return result;
        } // end N_ID
        case N_AR:{
            /* array access */
            int lineno = current->lineno;
            int err_flag = parent_stmt->err;

            Value* result = new Value;

            // array name
            string aname = string((char*)current->spec.ptr);
            STentry* aentry = stable[aname];

            Value* idx = exec(current->left, parent_stmt);
            
            if(idx->type != V_INT){
                result->type = V_UND;
                error(lineno, "type violation", &err_flag);
            }
            else if(aentry == NULL){
                // variable has not been declared
                result->type = V_UND;
                error(lineno, (aname+" has no value").c_str(), &err_flag);
            
            } else if(aentry->type == S_UNK){
                // variable has known type
                error(current->lineno, (aname+" has no value").c_str(), 
                    &(current->err));
                                
            } else if(aentry->type != S_ARR){
                result->type= V_UND;
                error(lineno, "type violation", &err_flag);
            }
            else{
                // member full name
                string mname = aname + '['+ to_string(idx->val.v_int) + ']';
                STentry* mentry = stable[mname];
                
                if(mentry == NULL){
                    // member doesn't exist
                    result->type = V_UND;
                    error(lineno, (mname + " has no value").c_str(), &err_flag);
                }
                else{
                    // copy value
                    valcpy(result, mentry->value);
                }
            }
            // update error bit
            parent_stmt->err = err_flag;
            
            delete idx;
            return result;
        } // N_AR;
        case N_FD:{
            /* field access */
            int lineno = current->lineno;
            int err_flag = parent_stmt->err;

            Value* result = new Value;

            string oname = string((char*)current->left);
            string fname = string((char*)current->right);
           
            string full_name = oname + '.' + fname;
            
            STentry* oentry = stable[oname];
            
            if(oentry == NULL){
                // object has not been declared
                result->type = V_UND;
                error(lineno, (oname+" has no value").c_str(), &err_flag);
            } else if(oentry->type == S_UNK){
                result->type = V_UND;
                error(current->lineno, (oname+" has no value").c_str(),
                        &(current->err));
                        
            } else if(oentry->type != S_OBJ){
                // variable is not an object 
                result->type = V_UND;
                error(lineno, "type violation", &err_flag);
            }
            else{
                STentry *fentry = stable[full_name];
                if(fentry == NULL){
                    result->type = V_UND;
                    error(lineno, (full_name + " has no value").c_str(), &err_flag);

                }
                else{
                    // copy value
                    valcpy(result, fentry->value);
                }
            }
            // update error bit
            parent_stmt->err = err_flag;
            return result;
        } // end N_FD
    
    } // End switch
}

/* Get a boolean value from Value */
int getBool(Value* value){
    // assume value->type != V_UND
    if(value->type == V_STR){
        return strlen(value->val.v_str);
    }
    else{
        return value->val.v_int;
    }
}

/* Clear the fields/members of obj/array with name */
void clear(string name){

    STentry* entry = stable[name];
    delete entry;
    
    string oprefix = name + '.';
    string aprefix = name + '[';
    
    auto itr = stable.begin();
    while (itr != stable.end()) {
        if (itr->first.substr(0, oprefix.size()) == oprefix) {
            // remove fields
            delete itr->second;
            stable.erase(itr++);
        } else if(itr->first.substr(0, aprefix.size()) == aprefix){
            // remove array members
            delete itr->second;
            stable.erase(itr++);
        }else {
            ++itr;
        }
   }
}

void valcpy(Value* dest, const Value* src){
    dest->type = src->type;
    if(src->type == V_STR){
        dest->val.v_str = strdup(src->val.v_str);
    }
    else{
        dest->val.v_int = src->val.v_int;
    }
}

/* Print Value v to stdout */
void print(Value* v){
    switch(v->type){
        case V_BOL:
        {
            /* Boolean value */
            if(v->val.v_int != 0){
                cout << "true";
            }
            else{
                cout << "false";
            }
            break;
        }
        case V_STR:
        {
            /* str value */
            if(strcmp(v->val.v_str, "<br />") == 0){
                cout << endl;
            }
            else{
                cout << v->val.v_str;
            }
            break;
        }
        case V_INT:
        {
            /* int value */
            cout << v->val.v_int;
            break;
        }
        case V_UND:
        {
            /* undefined */
            cout << "undefined";
            break;
        }
    }
}

/* Destructors */
STentry::~STentry(){
    if(type==S_SCA || type==S_ARR){
        delete value;
    }
}
Value::~Value(){
    if(type == V_STR){
        // free memory allocated by strdup()
        free(val.v_str);
    }
}
