CC = mpicc

all: life1d life1d_other life2d test pong

clean:
	rm -f *.o *sh.* life1d life2d hello_world pong
	rm -rf ./Vis_imdt/*
	rmdir  Vis_imdt

test: hello_mpi.o
	$(CC) -o hello_world hello_mpi.o pmpi.o -lssl

pong: mpi_pong.o pmpi.o
	$(CC) -o pong mpi_pong.o pmpi.o -lssl

life2d: life_2d_decom.o pmpi.o
	$(CC) -o life2d life_2d_decom.o pmpi.o -lssl

life1d: life_1d_decom.o pmpi.o
	$(CC) -o life1d life_1d_decom.o pmpi.o -lssl

life1d_other: life_other_1d.o pmpi.o
	$(CC) -o life1d_other life_other_1d.o pmpi.o -lssl

life2d: life_2d_decom.o pmpi.o
	$(CC) -o life2d life_2d_decom.o pmpi.o -lssl

life1d.o: life_1d_decom.c

life_other_1d.o: life_other_1d.c

life2d.o: life_2d_decom.c

hello_mpi.o: hello_mpi.c

mpi_pong.o: mpi_pong.c

pmpi.o: pmpi.c
	$(CC) -c pmpi.c
