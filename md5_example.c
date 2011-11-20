#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#include <mpi.h>
 
const char *string = "The quick brown fox jumped over the lazy dog's back";

#pragma weak MPI_Finalize = PMPI_Finalize
int PMPI_Finalize(){

/*
Dump all message info from memory to some file.  Need to get this to 
process 0 some how...

*/

}

#pragma weak MPI_Init = PMPI_Init
int PMPI_Init(int *argc, char ***argv){

/*
Start some sort of thread in here to hold all the messages in memory
until MPI_Finalize is called...

*/

}

#pragma weak MPI_Send = PMPI_Send
int PMPI_Send(void *buf, int count, MPI_Datatype datatype, int dest,
    int tag, MPI_Comm comm){
    
    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
 
    MD5(buf, sizeof(buf), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");
    
}

#pragma weak MPI_Recv = PMPI_Recv
int PMPI_Recv(void *buf, int count, MPI_Datatype datatype,
    int source, int tag, MPI_Comm comm, MPI_Status *status){

    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
 
    MD5(buf, sizeof(buf), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");


}

#pragma weak MPI_Barrier = PMPI_Barrier
int PMPI_Barrier(MPI_Comm comm){

    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
 
    MD5(comm, sizeof(comm), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");

}

#pragma weak MPI_Bcast = PMPI_Bcast
int PMPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
    int root, MPI_Comm comm){
    
    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
 
    MD5(buffer, sizeof(buffer), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");
    
}

