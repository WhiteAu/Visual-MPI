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



char cCurrentPath[FILENAME_MAX];
char *out_file = "/Vis_imdt/";

void *fd;
int numprocs;
int myid;



/* /\* supposedly sets the lock at open time. *\/ */

/* fl.l_type   = F_WRLCK;  /\* F_RDLCK, F_WRLCK, F_UNLCK    *\/ */
/* fl.l_whence = SEEK_SET; /\* SEEK_SET, SEEK_CUR, SEEK_END *\/ */
/* fl.l_start  = 0;        /\* Offset from l_whence         *\/ */
/* fl.l_len    = 0;        /\* length, 0 = to EOF           *\/ */
/* fl.l_pid    = getpid(); /\* our PID                      *\/ */

//fcntl(fd, F_SETLKW, &fl);  /* F_GETLK, F_SETLK, F_SETLKW */

enum data_type{
  FIRST_LINK = -1,
  SEND = 0,
  RECV,
  ISEND,
  IRECV,
  BCAST
};

struct _node{
  void *data;
  enum data_type type;
  struct _node *next;

};

typedef struct _node node;



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
  double fn_end_time;
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
 
 
static void init_q();
static void add_link (node *n);
static void init_node(node *n);
static void init_dir();
static int open_fp(void *fd);
static void pp_send(mpi_data_send *s, void *fd);
static void pp_recv(mpi_data_recv *s, void *fd);
static void print_q();
static char* itoa(int value, char* str, int radix);




/* our function queue to hold all 
* functions executed since last 
* pretty print dump
*/
node *queue;
int mpi_ob_size = MPI_MAX_OBJECT_NAME;

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
  init_q();
  init_dir();
  open_fp(fd);
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
  /* good ol' hack */
  ((node *)s)->type = SEND;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, s->comm, &mpi_ob_size);

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

  s->src = source;
  s->tag = tag;
  ((node *)s)->type = RECV;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, s->comm, &mpi_ob_size);
  
  n->data = s;

  return ret;
  
}

#pragma weak MPI_Barrier = SMPI_Barrier
int SMPI_Barrier(MPI_Comm comm){

    int i;
    unsigned char result[MD5_DIGEST_LENGTH];
	
    /* MD5(comm, sizeof(comm), result); */
    /* // output */
    /*   for(i = 0; i < MD5_DIGEST_LENGTH; i++) */
    /*     printf("%02x", result[i]); */
    /*   printf("\n"); */

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

static void init_q(){

  queue->next = NULL;
  queue->data = NULL;
  queue->type = FIRST_LINK;

  return;

}

static void add_link (node *n){
  node *skip = queue;


  /*get to end of list */
  while (skip->next){
    skip = skip->next;
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

/* Currently only accepts up to 2^32 nodes total */
static void init_dir(){
  int rank;
  char buf[32];
  char *txt = ".txt";
  int dir_check;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  buf = itoa(rank, buf, 10);

  GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));
  printf("The current working directory is %s", cCurrentPath);
  strcat(cCurrentPath, out_file);
  printf("The current working directory is %s", cCurrentPath);
  /* make our new directory, blindly.
     Don't care if it already exists or not.
 */
  dir_check = mkdir(cCurrentPath, 0111);

  strcat(cCurrentPath, buf);
  strcat(cCurrentPath, txt);

  return;

}


static int open_fp(void *fd){

  fd = fopen(cCurrentPath, "a");
  if (fd)
    return 0;
  else
    return -1;

}


static void pp_send(mpi_data_send *s, void *fd){
  int i;  
  int count;
  MPI_Datatype datatype;
  int dest;
  int tag;
  MPI_Comm comm;
  
  if (fd == NULL)
	fprintf(stderr,"pretty print of mpi_send data failed.\n");
  fprintf(fd, "TYPE: SEND\n");
  fprintf(fd, "COMM: %s\n", s->comm);
  fprintf(fd, "NODE: %d\n", s->rank);
  fprintf(fd, "ID: ");
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	fprintf(fd, "%02x", s->MD5[i]);
  fprintf(fd, "\n");
  fprintf(fd, "DTYPE: %s\n", s->datatype);
  fprintf(fd, "SIZE: %l\n", s->size);
  fprintf(fd, "DEST: %d\n", s->dest);
  fprintf(fd, "START TIME: %d\n\n", s->fn_start_time);
  fprintf(fd, "END TIME: %d\n\n", s->fn_end_time);

  return;
}


static void pp_recv(mpi_data_recv *s, void *fd){
  int i;  
  int count;
  MPI_Datatype datatype;
  int dest;
  int tag;
  MPI_Comm comm;
  
  if (fd == NULL)
	fprintf(stderr,"pretty print of mpi_send data failed.\n");
  fprintf(fd, "TYPE: SEND\n");
  fprintf(fd, "COMM: %s\n", s->comm);
  fprintf(fd, "NODE: %d\n", s->rank);
  fprintf(fd, "ID: ");
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	fprintf(fd, "%02x", s->MD5[i]);
  fprintf(fd, "\n");
  fprintf(fd, "DTYPE: %s\n", s->datatype);
  fprintf(fd, "SIZE: %l\n", s->size);
  fprintf(fd, "DEST: %d\n", s->src);
  fprintf(fd, "START TIME: %d\n\n", s->fn_start_time); 
  fprintf(fd, "END TIME: %d\n\n", s->fn_end_time);

  return;
}


static void print_q(){


}

    /* The Itoa code is in the puiblic domain */
char* itoa(int value, char* str, int radix) {
  char dig[] =
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz";
  int n = 0, neg = 0;
  unsigned int v;
  char* p, *q;
  char c;
  
  if (radix == 10 && value < 0) {
    value = -value;
    neg = 1;
  }
  v = value;
  do {
    str[n++] = dig[v%radix];
    v /= radix;
  } while (v);
  if (neg)
    str[n++] = '-';
  str[n] = '\0';
  for (p = str, q = p + n/2; p != q; ++p, --q)
    c = *p, *p = *q, *q = c;
  return str;
}
