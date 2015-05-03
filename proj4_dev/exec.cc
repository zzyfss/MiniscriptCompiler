#include "parser.h"
#include<iostream>
#include<string.h>
#include<string>

using namespace std;

// Global symbol table 
STable  glb_stable;

// function table
map<string, FTentry*> ftable;

void backtrace(Node* current, set<int> &lines); 

void updateReachDef(Node* current){
    
    /*
     * Since functions and assert never coexist
     * we can access global symbol table directly 
     */
    set<string>* uses = (set<string>*)current->uses;
    set<Node*>* reach_defs = (set<Node*> *) current->reach_defs;
    for(auto use : *uses){
        if(glb_stable[use]){
            set<Node*>* defs = glb_stable[use]->defs;
            reach_defs->insert(defs->begin(), defs->end()); 
        }

    }
}

void print(Value *v);

void print(STentry *entry);

int getBool(Value *v);

void clear(STable &stable);

void clear(STable &stable, string name);

void valcpy(Value* dest, const Value* src);

void error(int lineno, const char*  msg, int* err_flag){
    if(! *err_flag){
        cerr <<  "Line " << lineno << ", " << msg << endl;
        *err_flag = 1; 
    }
}

/* Interpret AST node */
Value* exec(Node* current, Node* parent_stmt, Node* ctrl, STable &loc_stable){
    if(current == NULL){
        return NULL;
    }
    switch(current->type){
        case N_PR:
        {
            /* program node */
            
            // process function definitions
            // fdefs at the beginning
            list<Node*> *fdefs = (list<Node*>*) current->left;
            for(auto fdef: *fdefs){
                exec(fdef, fdef, ctrl, loc_stable);
            }
            // fdefs at the end
            fdefs = (list<Node*>*) current->right;
            for(auto fdef: *fdefs){
                exec(fdef, fdef, ctrl, loc_stable);
            }

            // execute program's main body 
            Node* main = (Node*)current->spec.ptr;
            
            Value* val = exec(main, current, ctrl, loc_stable);

            delete val;
            // clear all entries in symbol table
            // loc_stable is the global one here
            // since N_PR is the root of AST
            clear(loc_stable);

            // Clear functions def
            auto itr = ftable.begin();

            while(itr != ftable.end()){
                delete itr->second;
                ftable.erase(itr++);
            }

            //program exits
            return NULL;
        }
        case N_FD:
        {
            /* function definition */

            // parameter list
            list<char*> *plist = (list<char*>*) current->spec.ptr;
            
            // number of parameter
            int pnum = plist->size();


            string fname = string((char*) current->left) + to_string(pnum);

            // check if the function is already defined
            if(ftable[fname] != NULL){
                // report type error
                error(current->lineno, "type violation",&(current->err));

            }
            else{
                // create a function entry
                FTentry* fentry = new FTentry;

                // initialize entry
                fentry->pnum = pnum;
                fentry->plist = plist;
                fentry->block = current->right;

                // put entry into function table
                ftable[fname] = fentry;

            }
            
            return NULL;

        }
        case N_FC:
        {
            /* function call */

            // argument list of function call
            list<Node*> *plist = (list<Node*> *) current->spec.ptr;
                
            string fname = string((char*) current->left) + 
                        to_string(plist->size());

            
            // return value of the function
            Value* reValue = new Value;

            FTentry *fentry = ftable[fname];     
            
            // check if the function is defined;
            if(fentry == NULL){
                // function doesn't exist;
                // report type error
                error(current->lineno, "type violation",&(current->err));
                // return undefined
                reValue->type = V_UND;
            }    
            else{
                // function call is valid

                // create a new local symbol table passed to callee
                STable new_stable;

                // iterate through plist and arguments
                auto pitr = fentry->plist->begin();
                auto aitr = plist->begin();

                while(aitr != plist->end()){
                    // get parameter name
                    string pname = string(*pitr);

                    // evaluate each argument
                    if((*aitr)->uses == NULL){
                        (*aitr)->uses = new set<string>;
                    }
                    if((*aitr)->reach_defs == NULL){
                        (*aitr)->reach_defs = new set<Node*>;
                    }
               
                    Value* arg_value = exec(*aitr, *aitr, ctrl, loc_stable);

                    // add an entry to new symbol table
                    STentry* s_entry = new STentry;
                    s_entry->type = S_SCA;
                    s_entry->value = arg_value;
                    s_entry->defs = new set<Node*>;

                    new_stable[pname] = s_entry;

                    // increment iterators
                    aitr++;
                    pitr++;
                }

                // execute function statements
                Value* result = exec(fentry->block, fentry->block, ctrl, new_stable);
                delete result;
                
                // retrieve return value
                STentry* ret = new_stable[RET_ADDR];

                if(ret == NULL){
                    reValue->type = V_UND;
                }
                else{
                    // type checking is done in callee
                    valcpy(reValue, ret->value);

                }

                // delete all entries in the stable created for callee
                clear(new_stable);


            }// end if function exists

            return reValue;

        } // end function call node
        case N_RE:
        {
            /* Return node */
            if(current->uses == NULL){
                current->uses = new set<string>;
            }

            // evaluate return expression
            Value* ret = exec(current->left, current, ctrl, loc_stable);
            
            // create a return entry
            STentry* entry = new STentry;
            entry->type = S_SCA;
            entry->value = ret;
            entry->defs = NULL; 
            
            // add the entry into local symbol table
            loc_stable[RET_ADDR] = entry; 

            // Create a V_RET to indicate a return statement to parent block
            Value* result = new Value;
            result->type = V_RET;

            return result; 

        }
        case N_AT:
        {
            /* Assert node */

            Value* result = NULL;

            current->ctrl = ctrl;
            
            // init uses set
            if(current->uses == NULL){
                current->uses = new set<string>;
            }
            
            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }


            // evaluate assert condition
            Value* cdt = exec(current->left, current, ctrl, loc_stable);
           
            updateReachDef(current);
            
            if(cdt->type == V_UND || getBool(cdt) == 0){
                // assert fails

                // Generate diagnosis report
                cerr << "Diagnosis Report" << endl; 
                set<int> lines;

                backtrace(current, lines);

                for(auto i: lines){
                    cerr << "Line " << i << endl;
                }

                /*
                 * Assuming there are no function calls 
                 * when a program have assertions
                 * Use V_RET to exit the program
                 */
                result = new Value;
                result->type = V_RET;

            }
            // do nothing if assertion is valid
            
            delete cdt;
            return result;

        }
        case N_BL: 
        {
            /* Block node */

            list<Node*> *slist = (list<Node*> *) current->spec.ptr;
            
            // iterate statement list
            for(auto stmt: *slist){
                Value* v = exec(stmt, stmt, ctrl, loc_stable);
                if( v != NULL && 
                    (v->type == V_BRE 
                        || v->type == V_CON
                        || v->type == V_RET)){
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
            current->ctrl = ctrl;

            list<Node*> *plist = (list<Node*> *) current->spec.ptr;
            
            if(plist == NULL){
                // no arguments for write()
                return NULL;
            }

            list<Value*> *vlist = new list<Value*>;
            for(auto para: *plist){
                /* Each parameter is considered a statement 
                 * for error reporting 
                 */
                if(para->uses == NULL){
                    para->uses = new set<string>;
                }
                Value* v = exec(para, para, ctrl, loc_stable);
                vlist->push_back(v);
            }

            auto itr = vlist->begin();
            
            // print values
            while(itr != vlist->end()){
                print(*itr);
                
                delete *itr;
                // remove 
                vlist->erase(itr++);
            }

            delete vlist; 
            
            return NULL;
        }
        case N_IF:
        {
            /* if statement */
            Node* cond = (Node*) current->spec.ptr;

            // create a uses set to store variable name in condition
            
            if(cond->uses == NULL){
                cond->uses = new set<string>; 
            }
            if(cond->reach_defs == NULL){
                cond->reach_defs = new set<Node*>;
            }
            
            Value* result = exec(cond, cond, ctrl, loc_stable);
            current->ctrl = ctrl;

            updateReachDef(cond);  

            if(result->type == V_UND){
                error(cond->lineno, "condition unknown",&(cond->err));
                // skips entire if statement
                delete result;
                return NULL;
            }
            else if(getBool(result)){
                delete result;
                return exec(current->left, NULL, cond, loc_stable);
            }
            else{
                delete result;
                return exec(current->right, NULL, cond, loc_stable);
            }
        } // end N_IF
        case N_WD:
        {
            /* while statement */
            Node* cond = current->left;
            
            if(cond->uses == NULL){
                cond->uses = new set<string>; 
            }
            if(cond->reach_defs == NULL){
                cond->reach_defs = new set<Node*>;
            }
            
            Value* result = NULL;
            current->ctrl = ctrl;
                
            while(1){
                delete result;
                // evaluate condition
                result=exec(cond, cond, ctrl, loc_stable);
                
                updateReachDef(cond);
                
                if(result->type == V_UND){
                    error(cond->lineno, "condition unknown",&(cond->err));
                
                    //  skips entire while statement
                    break;         
                }
                if( getBool(result) == 0){
                    // condition is false
                    break;
                }
                // execute body
                Value* value = exec(current->right, NULL, cond, loc_stable);
                if(value != NULL && 
                    (value->type == V_BRE || value->type == V_RET)){
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
        
            if(cond->uses == NULL){
                cond->uses = new set<string>; 
            }
            if(cond->reach_defs == NULL){
                cond->reach_defs = new set<Node*>;
            }

            current->ctrl = ctrl; 
            Value* result = NULL;
            
            while(1){
                // execute body first
                Value* value = exec(current->right, NULL, cond, loc_stable);
                if(value != NULL && 
                    (value->type == V_BRE || value->type == V_RET )){
                    // Break
                    delete value;
                    break;
                }
                
                delete result;

                // evaluate condition
                result=exec(cond, cond, ctrl, loc_stable);

                updateReachDef(cond);
                
                if(result->type == V_UND){
                    error(cond->lineno, "condition unknown",&(cond->err));
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
            /* Variable declaration (w/o init) */
            
            current->ctrl = ctrl;
            
            if(current->uses == NULL){
                current->uses = new set<string>;
            }
            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }

            string vname = string((char*) current->spec.ptr);
            
            // clear old fields (array members) if any
            clear(loc_stable, vname);
            /* Declare without initialization */
            STentry* entry = new STentry;
            entry->type = S_UNK;
            entry->defs = new set<Node*>;
            entry->defs->insert(current);

            loc_stable[vname] = entry;
            return NULL;
        }
        case N_SDE:{
            /* Scalar declaration */
        
            current->ctrl = ctrl;
            
            if(current->uses == NULL){
                current->uses = new set<string>;
            }

            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }

            string vname = string((char*) current->spec.ptr);
            
            // clear old fields (array members) if any
            clear(loc_stable, vname);
            
            STentry* entry = new STentry;
            entry->type = S_SCA;
            entry->defs = new set<Node*>;
            entry->defs->insert(current);

            entry->value = exec(current->right, current, ctrl, loc_stable);
        
            updateReachDef(current);
            
            loc_stable[vname] = entry;
                      
            return NULL;
        } 
        case N_ODE:{
            /* Object declaration */

            current->ctrl = ctrl;
            
            if(current->uses == NULL){
                current->uses = new set<string>;
            }

            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }

            string oname = string((char*) current->spec.ptr);
            // clear old fields (array members) if any
            clear(loc_stable, oname);
            STentry* entry = new STentry;
            entry->type = S_OBJ;
            entry->defs = new set<Node*>;
            entry->defs->insert(current);

            // insert new entry
            loc_stable[oname] = entry;

            // field initialization list
            list<Node*> *flist = (list<Node*>*) current->right;
            if(flist != NULL){
                for(auto ifd : *flist){
                    exec(ifd, current, ctrl,  loc_stable);
                }
            }


            return NULL;
        }
        case N_IFD:{
            /* Field initialization */
            
            current->ctrl = ctrl;
            
            if(current->uses == NULL){
                current->uses = new set<string>;
            }
            
            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }
            
            // field name
            string fname = string((char*)current->spec.ptr);
            // obj name
            string oname = string((char*) parent_stmt->spec.ptr);

            // entry name
            string ename = oname + "." + fname;

            STentry *entry = new STentry;
            entry->type = S_SCA;
            entry->defs = new set<Node*>;
            entry->defs->insert(current);

            entry->value = exec(current->right, parent_stmt, ctrl, loc_stable);

            updateReachDef(current);
            // insert new entry to symbol table
            loc_stable[ename] = entry;

            return NULL;
        }
        case N_ADE:{
            /* Array declaration node */ 

            current->ctrl = ctrl;
            if(current->uses == NULL){
                current->uses = new set<string>;
            }
            
            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }

            string aname = string((char*)current->spec.ptr);
            // clear old variable
            clear(loc_stable, aname);
        
            /* Array declaration */
            STentry* aentry = new STentry;
            aentry->type = S_ARR;
            aentry->defs = new set<Node*>;
            aentry->defs->insert(current);

            // insert new entry
            loc_stable[aname] = aentry;
        
            // array size
            int asize = 0;

            list<Node*>* alist = (list<Node*>*) current->right;
            
            if(alist != NULL){
                // member init
                for(auto exp: *alist){
                    Value* avalue = exec(exp, current, ctrl, loc_stable);
                    
                    updateReachDef(current);

                    // member name = aname[index]
                    string mname = aname + '[' + to_string(asize) + ']';
                    // create an entry
                    STentry* mentry = new STentry;
                    mentry->defs = new set<Node*>; 
                    mentry->defs->insert(exp);
                    mentry->type = S_SCA;
                    mentry->value = avalue;

                    // insert entry
                    loc_stable[mname] = mentry;

                    // increment size
                    asize++;
                }
            }

            // set array size
            Value* size = new Value;
            size->val.v_int = asize;
            size->type = V_INT;

            aentry->value = size;
            
            return NULL;
        } // end N_ADE
        case N_AS:{
            /* Assign statement */

            current->ctrl = ctrl;
            
            if(current->uses == NULL){
                current->uses = new set<string>;
            }
            
            if(current->reach_defs == NULL){
                current->reach_defs = new set<Node*>;
            }
            
            // Evaluate RHS first
            Value* value = exec(current->right, current, ctrl,  loc_stable);
            
            updateReachDef(current); 
            
            Node* left = current->left;

            string var_name;

            // flag indicate if variable is local 
            int isLocal = 1;

            if(left->type == N_ID){
                var_name = string((char*)left->spec.ptr);
                
                if(loc_stable[var_name] == NULL){
                    isLocal = 0;
                }
            }
            else if(left->type == N_FA){
                // object name
                string oname = string((char*)left->left);
                // field name
                string fname = string((char*)left->right);
                var_name = oname + '.' + fname;
                
                // check if obj exists in local stable
                STentry* oentry = loc_stable[oname];
                
                if(oentry == NULL){
                    // try to retrieve it from global symbol table
                    oentry = glb_stable[oname];
                    isLocal = 0; 
                }   
                
                if(oentry == NULL){ 
                    error(current->lineno, (oname + " undeclared").c_str(),
                        &(current->err));
                    oentry = new STentry;
                    oentry->type = S_OBJ;
                    oentry->defs = new set<Node*>;
                    oentry->defs->insert(current);
                    
                    // insert into local table
                    loc_stable[oname] = oentry;
                } else if (oentry->type != S_OBJ){
                    // entry is not an object
                    error(current->lineno, "type violation", &(current->err));
                    delete value;
                    return NULL;
                }
            } // end type == N_FA
            else if(left->type == N_AR){
                // array name
                string aname = string((char*)left->spec.ptr);
                
                // index
                Value* idx = exec(left->left, current, ctrl, loc_stable);
                
                updateReachDef(current);

                if(idx->type != V_INT){
                    error(current->lineno, "type violation", &(current->err));
                    delete idx;
                    delete value;
                    return NULL;
                }
                else{
                    // array member full name
                    var_name = aname + '[' + to_string(idx->val.v_int) + ']';
                    
                    // look up entry in local symbol table first
                    STentry *aentry = loc_stable[aname];
                    
                    if(aentry == NULL){
                        // loop up entry in global stable
                        aentry = glb_stable[aname];
                        isLocal = 0;
                    }

                    if(aentry == NULL){
                        // array doesn't exist
                        error(current->lineno, 
                        (aname + " undeclared").c_str(),
                        &(current->err));
                        
                        // add new array entry
                        aentry = new STentry;
                        aentry->type = S_ARR;
                        aentry->defs = new set<Node*>;
                        aentry->defs->insert(current);

                        loc_stable[aname] = aentry;
                        /* Need size? nah */
                        Value* size = new Value;
                        size->type = V_INT;
                        size->val.v_int = idx->val.v_int + 1;
                    } else if(aentry->type != S_ARR){
                        error(current->lineno, "type violation", &(current->err));
                        delete value;
                        delete idx;
                        return NULL;
                    }
                }
            } // left->type == N_AR
            
          
            
            STentry* entry = NULL;
            if(isLocal){
                entry = loc_stable[var_name];
            }
            else{
                entry = glb_stable[var_name];
            }
            
            if(entry == NULL){
                
                // Variable has not been declared!
                if(left->type == N_ID){
                    error(current->lineno, 
                        (var_name + " undeclared").c_str(), 
                        &(current->err));
                }

                // Add a new entry into local stable
                entry = new STentry;
                entry->type = S_SCA;
                entry->value = value;
                entry->defs = new set<Node*>;
                entry->defs->insert(current);

                loc_stable[var_name] = entry;
            }
            else if (entry->type == S_SCA){
                entry->value = value;
                entry->defs->clear();
                entry->defs->insert(current);
            }
            else if (entry->type == S_UNK){
                entry->value = value;
                entry->type = S_SCA;
                entry->defs->clear();
                entry->defs->insert(current);
            }
            else{
                // cannot assigne values to object/array/function
                error(current->lineno, "type violation", &(current->err));
                delete value;
            }
            
            /* debug uses set
            set<string> * uses = (set<string>*) current->uses;
            for(auto use: *uses){
                cerr << use + " ";
            }
            cerr << endl;
            // */

            return NULL;
        }
        case N_EX:
        {
            current->ctrl = ctrl;

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

            Value* lop = exec(current->left, parent_stmt, ctrl, loc_stable);
            
            /* short circuiting */ 
            if(lop->type == V_UND){
                result->type = V_UND; 
                delete lop;
                return result;
            }
            else if(current->spec.etype == E_AND ){
                result->type = V_BOL;

                if(lop->type == V_STR){
                    // cast string to boolean
                    lop->type = V_BOL;
                    lop->val.v_int = strlen(lop->val.v_str) != 0;
                }

                // Evaluate result
                if(lop->val.v_int == 0){
                    // left operand is false
                    result->val.v_int = 0;

                    // don't need to evaluate right operand
                    delete lop;
                    return result;
                }
            }
            else if(current->spec.etype == E_OR){
                result->type = V_BOL;

                if(lop->type == V_STR){
                    // cast string to boolean
                    lop->type = V_BOL;
                    lop->val.v_int = strlen(lop->val.v_str) != 0;
                }

                // Evaluate result
                if(lop->val.v_int != 0){
                    // left operand is true

                    result->val.v_int = 1;

                    // don't need to evaluate right operand
                    delete lop;
                    return result;
                }                       
            } // end short circuiting
            

            Value* rop = exec(current->right, parent_stmt, ctrl, loc_stable);

            // Handle undefined operands
            if(lop->type == V_UND || 
                rop != NULL && rop->type == V_UND) {
                result->type = V_UND;
                delete lop;
                delete rop;
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
            
            current->ctrl = ctrl;
            /* copy value */ 
            Value* src = (Value*) current->spec.ptr;
            Value* result = new Value;
            
            valcpy(result, src);
            
            return result;
        }
        case N_ID:
        {
            /* Variable access node */ 

            current->ctrl = ctrl;

            string vname = string((char*) current->spec.ptr);
            int lineno = current->lineno;
            int err_flag = parent_stmt->err;
            
            Value* result = new Value;

            // Retreive associated entry in local symbol table
            STentry* entry = loc_stable[vname];

            if(entry == NULL){
                entry = glb_stable[vname]; 
            }
            
            if(entry == NULL){
                // variable has not been declared (value error)
                error(lineno, (vname + " has no value").c_str(), &err_flag);
                
                result->type = V_UND;
            }
            else if(entry->type == S_UNK){
                // variable has been declared but not written (value error)
                error(lineno, (vname + " has no value").c_str(), &err_flag);
                result->type = V_UND;
                set<string>* uses = (set<string>*) parent_stmt->uses;
                uses->insert(vname);



            }else if(entry->type == S_OBJ){
                // object can not be a variable
                error(lineno, "type violation", &err_flag);

                result->type = V_UND;

                set<string>* uses = (set<string>*) parent_stmt->uses;
                uses->insert(vname);
            }
            else if(entry->type == S_ARR){
                error(lineno, "type violation", &err_flag);
                result->type = V_UND;

                set<string>* uses = (set<string>*) parent_stmt->uses;
                uses->insert(vname);
            }
            else {
                // copy value 
                valcpy(result, entry->value);
                
                set<string>* uses = (set<string>*) parent_stmt->uses;
                uses->insert(vname);

            }
            
            // update error flag
            parent_stmt->err = err_flag;
            
            return result;
        } // end N_ID
        case N_AR:{
            /* array access */
            
            current->ctrl = ctrl; 
            
            int lineno = current->lineno;
            int err_flag = parent_stmt->err;

            Value* result = new Value;

            // array name
            string aname = string((char*)current->spec.ptr);
            STentry* aentry = loc_stable[aname];
            int isLocal = 1;
            
            if(aentry == NULL){
                // retrieve from global stable
                aentry = glb_stable[aname];
                isLocal = 0; 
            }

            Value* idx = exec(current->left, parent_stmt, ctrl, loc_stable);
            
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
                
                STentry* mentry = NULL;
                if(isLocal){
                    mentry = loc_stable[mname];
                }
                else{
                    mentry = glb_stable[mname];
                }

                if(mentry == NULL){
                    // member doesn't exist
                    result->type = V_UND;
                    error(lineno, (mname + " has no value").c_str(), &err_flag);
                }
                else{
                    // copy value
                    valcpy(result, mentry->value);
                
                    set<string>* uses = (set<string>*) parent_stmt->uses;

                    uses->insert(mname);
                
                }
            }
            // update error bit
            parent_stmt->err = err_flag;
            
            delete idx;
            return result;
        } // N_AR;
        case N_FA:
        {   /* field access */
            
            current->ctrl = ctrl; 

            int lineno = current->lineno;
            int err_flag = parent_stmt->err;

            Value* result = new Value;

            string oname = string((char*)current->left);
            string fname = string((char*)current->right);
           
            string full_name = oname + '.' + fname;
            
            STentry* oentry = loc_stable[oname];
            int isLocal = 1;

            if(oentry == NULL){
                oentry = glb_stable[oname];
                isLocal = 0;
            }
            
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
                STentry *fentry = NULL;
                if(isLocal){
                    fentry = loc_stable[full_name];
                }
                else{
                    fentry = glb_stable[full_name];
                }

                if(fentry == NULL){
                    result->type = V_UND;
                    error(lineno, (full_name + " has no value").c_str(), &err_flag);

                }
                else{
                    // copy value
                    set<string>* uses = (set<string>*) parent_stmt->uses;
                    uses->insert(full_name);

                    valcpy(result, fentry->value);
                }
            }
            // update error bit
            parent_stmt->err = err_flag;
            return result;
        } // end N_FA
    
    } // End switch
}

/* DFS on dependency graph */
void backtrace(Node* current, set<int> &lines){

    if(current == NULL){
        return;
    }

    pair<set<int>::iterator,bool> ret;
    ret = lines.insert(current->lineno); 

    // check if current node is visited
    if(ret.second == true){
        //  has not been visisted

        set<Node*>* defs = (set<Node*>*) current->reach_defs;
        for(auto def: *defs){
            backtrace(def, lines);
        }         
    
        // also need to visit control statement
        backtrace(current->ctrl, lines); 
    }
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

/* Delete all entries in a STable */
void clear(STable &stable){
    auto itr = stable.begin();

    while(itr != stable.end()){
        delete itr->second;
        stable.erase(itr++);
    }
}

/* Clear the fields/members of obj/array with name */
void clear(STable &stable, string name){

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
    if(v == NULL){
        return;
    }
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

/* help debugging */
void print(STentry* entry){
    if(entry == NULL){
        return;
    }
    switch(entry->type){
        case S_OBJ: 
        {   
            cout << "object" << endl;
            break;
        }
        case S_ARR:
        {
            cout << "array" << endl;
            break;
        }
        case S_SCA:
        {
            cout << "scalar" << endl;
            break;
        }
        case S_UNK:{
            cout << "unknown" << endl;
            break;

        }
    }
}

/* Destructors */
STentry::~STentry(){
    if(type==S_SCA || type==S_ARR){
        delete value;
    }
    if(defs != NULL){
        delete defs;
    }
}
Value::~Value(){
    if(type == V_STR){
        // free memory allocated by strdup()
        free(val.v_str);
    }
}

FTentry::~FTentry(){
    // free parameter list
    for(char* pname : *plist){
        free(pname);
    }
    
    delete plist;
}
