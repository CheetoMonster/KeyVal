
F := KeyVal\
     KeyVal_load

FLAGS := -O0 -g

WARNS := -Wshadow

default: test

main: $(F:KeyVal%=KeyVal%.o) main.o
	@echo
	@echo --- $@..
	clang $(FLAGS) $(WARNS) $^ -fPIC -Wall -o $@
	@echo
	@echo --- ok!
	@echo

#test: $(F:KeyVal%=KeyVal%.o) test.o
#	clang $(FLAGS) $(WARNS) $^ -fPIC -Wall -o $@
test: libKeyVal.so test.o
	@echo
	@echo --- $@..
	clang $(FLAGS) $(WARNS) test.o -L. -lKeyVal -fPIC -Wall -o $@
	@echo
	@echo --- ok!
	@echo


%.o: %.c
	@echo
	@echo --- $@..
	clang $(FLAGS) $(WARNS) -fPIC -Wall -c $< -o $@

libKeyVal.so: $(F:KeyVal%=KeyVal%.o)
	@echo
	@echo --- $@..
	clang $^ -shared -o $@


clean:
	rm -f *.o main


