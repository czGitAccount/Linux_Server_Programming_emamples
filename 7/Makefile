TAR1 = webSrv
TAR2 = stress_test
OBJ1 = main.o http_conn.o 
OBJ2 = stress_test.o
++ := g++

all: webSrv stress_test

$(TAR1) : $(OBJ1)
	$(++) $(OBJ1) -o $(TAR1) -lpthread
$(TAR2): $(OBJ2)
	$(++) $(OBJ2) -o $(TAR2)

$(OBJ1):http_conn.h locker.h threadpool.h 
$(OBJ2):stress_test.cpp

cleanO:
	rm -rf $(OBJ1) $(OBJ2) 
cleanE:
	rm -rf $(TAR1) $(TAR2) 

