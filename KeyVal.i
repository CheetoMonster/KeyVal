

%{
#include "KeyVal.h"
%}

%include "KeyVal.h"

%include cpointer.i
%pointer_functions(struct KeyVal*, KeyVal_ptr_ptr)
%pointer_functions(char*, char_ptr_ptr)
%pointer_functions(unsigned char, bool_ptr)
%pointer_functions(unsigned long, long_ptr)
%pointer_functions(char**, char_ptr_ptr_ptr)

%include carrays.i
%array_functions(char *, char_ptr_arr)

