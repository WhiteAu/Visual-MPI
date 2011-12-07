CC = mpicc


test: hello_mpi.o pmpi.o
	$(CC) -o test hello_mpi.o pmpi.o -lssl

hello_mpi.o: hello_mpi.c

pmpi.o:
	$(CC) -c pmpi.c