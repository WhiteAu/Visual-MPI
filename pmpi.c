#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <mpi.h>

#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
 #endif

#define DEF_MODE 0666

char cCurrentPath[FILENAME_MAX];
GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
printf ("The current working directory is %s", cCurrentPath);

char *out_file = "/Vis_imdt/";
strcat(cCurrentPath, out_file);
printf ("The current working directory is %s", cCurrentPath);

int *fd;
int numprocs;
int myid;

/* supposedly sets the lock at open time. */

fl.l_type   = F_WRLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
fl.l_start  = 0;        /* Offset from l_whence         */
fl.l_len    = 0;        /* length, 0 = to EOF           */
fl.l_pid    = getpid(); /* our PID                      */

//fcntl(fd, F_SETLKW, &fl);  /* F_GETLK, F_SETLK, F_SETLKW */

enum data_type{
  FIRST_LINK = -1,
  SEND = 0,
  RECV,
  ISEND,
  IRECV,
  BCAST,

}


typedef struct _node{
  void *data;
  data_type  type;
  node *next;

}node;



/******************************************
*
*This struct is currently unused...
*Leaving it in just in case we decide to try and
*cut down on pretty print functions... 
* -ASW
*****************************************/
typedef struct _mpi_data{
  node n;
  int rank;
  unsigned char MD5[MD5_DIGEST_LENGTH];
  double fn_start_time;
  double fn_end_time;
  char comm[MPI_MAX_OBJECT_NAME];
  int target; /*source or dest... depending on MPI call.  Whatever the relation to the other node is... */

} mpi_data;

typedef struct _mpi_data_send{
  node n;
  int rank;
  unsigned char MD5[MD5_DIGEST_LENGTH];
  double fn_start_time;
  double fn_end_time
  char comm[MPI_MAX_OBJECT_NAME];
  int dest;

  int count;
  MPI_Datatype datatype;
  int size;
  int tag;
  int comm_len;
  
 }mpi_data_send;
 
typedef struct _mpi_data_recv{
  node n;
  int rank;
  unsigned char MD5[MD5_DIGEST_LENGTH];
  double fn_start_time;
  double fn_end_time;
  char comm[MPI_MAX_OBJECT_NAME];
  int src;
    
  int count;
  MPI_Datatype datatype;
  int size;
  int tag;
 
 
 }mpi_data_recv;
 
typedef struct _mpi_data_bcast{
  node n;
  int count;
  MPI_Datatype datatype;
  int root;
  MPI_Comm comm; 
 
 }mpi_data_bcast;
 
 

static void add_link (node *n);
static void init_node(node *n);
static int open_fp(int *fd);
static void pp_send(mpi_data_send *s, int *fd);



/* our function queue to hold all 
* functions executed since last 
* pretty print dump
*/
node *queue;
queue->next = NULL;
queue->data = NULL;
queue->data_type = FIRST_LINK;




#pragma weak MPI_Finalize = SMPI_Finalize
int SMPI_Finalize(){

/*
Dump all message info from memory to some file.  Need to get this to 
process 0 some how...

*/

}

#pragma weak MPI_Init = SMPI_Init
int SMPI_Init(int *argc, char ***argv){
  int ret;
  /* this returns the current working directory */

  ret = PMPI_Init(argc, argv);


  return ret;

/*
Start some sort of thread in here to hold all the messages in memory
until MPI_Finalize is called...

*/

}

#pragma weak MPI_Send = SMPI_Send
int SMPI_Send(void *buf, int count, MPI_Datatype datatype, int dest,
    int tag, MPI_Comm comm){
  
  node *n = malloc (sizeof(node));
  mpi_data_send *s = malloc(sizeof(mpi_data_send));
  int i;
  unsigned char id[MD5_DIGEST_LENGTH];
  int extent;
  s->fn_start_time = MPI_WTime();
  /* pass call along to PMPI_Send, the core fn call */
  int ret    = PMPI_Send(buf, count, datatype, dest, tag, comm);    
  s->fn_end_time = MPI_Wtime();      /* end time          */ 
  
  init_node(n);
  MPI_Type_size(datatype, &extent);  /* Compute size */ 
  s->size = count*extent; 
  MD5(buf, sizeof(buf), s->MD5);
    // output
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	printf("%02x",  s->MD5[i]);
  printf("\n");

  s->dest = dest;
  s->tag = tag;
  (node)s->type = SEND;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, &s->comm, &s->comm_len);

  n->data = s;

  return ret;  
    
}

#pragma weak MPI_Recv = SMPI_Recv
int SMPI_Recv(void *buf, int count, MPI_Datatype datatype,
    int source, int tag, MPI_Comm comm, MPI_Status *status){

  node *n = malloc (sizeof(node));
  mpi_data_recv *s = malloc(sizeof(mpi_data_recv));
  int i;
  unsigned char id[MD5_DIGEST_LENGTH];
  int extent; 
  s->fn_start_time = MPI_WTime();
  /* pass call along to PMPI_Recv, the core fn call */
  int ret    = PMPI_Recv(buf, count, datatype, source, tag, comm, status);    
  s->fn_end_time = MPI_WTime();

  init_node(n);
  MPI_Type_size(datatype, &extent);  /* Compute size */ 
  s->size = count*extent; 
  MD5(buf, sizeof(buf), s->MD5);
  /* output to test MD5 */
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	printf("%02x",  s->MD5[i]);
  printf("\n");

  s->src = src;
  s->tag = tag;
  (node)s->type = RECV;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, &s->comm, &s->comm_len);
  
  n->data = s;

  return ret;
  
}

#pragma weak MPI_Barrier = SMPI_Barrier
int SMPI_Barrier(MPI_Comm comm){

    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
	
    MD5(comm, sizeof(comm), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");

}

#pragma weak MPI_Bcast = SMPI_Bcast
int SMPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
    int root, MPI_Comm comm){
    
    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
 
    MD5(buffer, sizeof(buffer), result);
    // output
      for(i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", result[i]);
      printf("\n");
    
}
/*
#pragma weak MPI_Comm_rank = PMPI_Comm_rank
int PMPI_Comm_rank(MPI_Comm comm, int *rank){


}
*/
/**********************
 *
 * STATIC FUNCTIONS ***
 *
 *********************/

static void add_link (node *n){
  node *skip = queue;

  while (skip->next){
	skip = skip->next
  }
  skip->next = n;

  return;

}

static void init_node(node *n){
  n = malloc(sizeof(node));
  n->data = NULL;
  n->next = NULL;

  return;

}

static int open_fp(int *fd){
  return (fd = open("filename", O_WRONLY | O_CREAT | O_APPEND, DEF_MODE));

}


static void pp_send(mpi_data_send *s, int *fd){
  int i;  
  int count;
  MPI_Datatype datatype;
  int dest;
  int tag;
  MPI_Comm comm;
  
  if (fd == NULL)
	fprintf(2,"pretty print of mpi_send data failed.\n");
  fprintf(fd, "TYPE: SEND\n");
  fprintf(fd, "COMM: %s\n", (int) s->comm);
  fprintf(fd, "NODE: %d\n", s->rank);
  fprintf(fd, "ID: ");
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	fprintf(fd, "%02x", s->MD5[i]);
  fprintf(fd, "\n");
  fprintf(fd, "DTYPE: %s\n", s->datatype);
  fprintf(fd, "SIZE: %l\n", s->size);
  fprintf(fd, "DEST: %d\n", s->dest);
  fprintf(fd, "TIME: %d\n\n", s->fn_time); 

  return;
}
