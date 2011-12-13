#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> /* for mkdir and stat check*/
#include <sys/types.h> /* for mkdir */
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <mpi.h>
#include <time.h>
#include <errno.h>

#ifdef WINDOWS
    #include <direct.h>
    #define GetCurrentDir _getcwd
#else
    #include <unistd.h>
    #define GetCurrentDir getcwd
#endif



char cCurrentPath[FILENAME_MAX];
char *out_file = "Vis_imdt/";
char file_name[FILENAME_MAX];

void *fd;
int numprocs;
int myid;
double init_start; /*used to calculate time relative to when MPI_Init is called... */



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
  char datatype[MPI_MAX_OBJECT_NAME];
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
  char datatype[MPI_MAX_OBJECT_NAME];
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
static void pp_send(mpi_data_send *s);
static void pp_recv(mpi_data_recv *s);
static void print_q();
char* itoa(int value, char* str, int radix);
static double get_time();



/* our function queue to hold all 
* functions executed since last 
* pretty print dump
*/
node *queue;
int mpi_ob_size = MPI_MAX_OBJECT_NAME;

#pragma weak MPI_Finalize = SMPI_Finalize
int SMPI_Finalize(){


  int ret;
  /*
	Dump all message info from memory to some file.  Need to get this to 
	process 0 some how...
	
  */

  ret = PMPI_Finalize();
  print_q();
  //fprintf(stderr, "Finalize Stuff is good.\n");
  return ret;



}

#pragma weak MPI_Init = SMPI_Init
int SMPI_Init(int *argc, char ***argv){
  int ret;
  /* this returns the current working directory */
  init_q();
 
  ret = PMPI_Init(argc, argv);
 
  init_dir();
  //open_fp(fd);
  init_start = get_time();
  //fprintf(stderr, "Init Stuff is good.\n");
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
  int extent, type_extent;

  s->fn_start_time = get_time() - init_start;
  /* pass call along to PMPI_Send, the core fn call */
  int ret    = PMPI_Send(buf, count, datatype, dest, tag, comm);    
  s->fn_end_time = get_time() - init_start;      /* end time          */ 
  
  init_node(n);
  MPI_Type_get_name(datatype, s->datatype, &type_extent);
  MPI_Type_size(datatype, &extent);  /* Compute size */ 
  s->size = count*extent; 
  //fprintf(stderr, "in Send, count is : %d  extent is: %d size is : %d\n",count, extent, s->size);
  MD5(buf, s->size, s->MD5);
  s->dest = dest;
  s->tag = tag;
  /* good ol' hack */
  ((node *)s)->type = SEND;
  n->type = SEND;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, s->comm, &mpi_ob_size);

  n->data = s;
  //pp_send(s, fd);
  add_link(n);
  return ret;  
    
}

#pragma weak MPI_Recv = SMPI_Recv
int SMPI_Recv(void *buf, int count, MPI_Datatype datatype,
    int source, int tag, MPI_Comm comm, MPI_Status *status){

  node *n = malloc (sizeof(node));
  mpi_data_recv *s = malloc(sizeof(mpi_data_recv));
  int i;
  unsigned char id[MD5_DIGEST_LENGTH];
  int extent, type_extent; 
  s->fn_start_time = get_time() - init_start;
  /* pass call along to PMPI_Recv, the core fn call */
  int ret    = PMPI_Recv(buf, count, datatype, source, tag, comm, status);    
  s->fn_end_time = get_time() - init_start;

  init_node(n);
  MPI_Type_get_name(datatype, s->datatype, &type_extent);
  MPI_Type_size(datatype, &extent);  /* Compute size */ 
  s->size = count*extent; 
  //fprintf(stderr, "In Recv, count is : %d  extent is: %d size is : %d\n",count, extent, s->size);
  MD5(buf,s->size, s->MD5);
  /* output to test MD5 */
  /*
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	printf("%02x",  s->MD5[i]);
  printf("\n");
  */
  s->src = source;
  s->tag = tag;
  ((node *)s)->type = RECV;
  n->type = RECV;
  MPI_Comm_rank( comm, &s->rank);
  MPI_Comm_get_name(comm, s->comm, &mpi_ob_size);
  
  n->data = s;
  //pp_recv(s, fd);
  add_link(n);
  //fprintf(stderr, "Recv Stuff is good.\n");
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
	i = PMPI_Barrier(comm);
	
	return i;
}

#pragma weak MPI_Bcast = SMPI_Bcast
int SMPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
    int root, MPI_Comm comm){
  int ret;
  int i;
  unsigned char result[MD5_DIGEST_LENGTH];
  
  MD5(buffer, sizeof(buffer), result);
  // output
 /*  for(i = 0; i < MD5_DIGEST_LENGTH; i++) */
/* 	printf("%02x", result[i]); */
/*   printf("\n"); */
	
  ret = PMPI_Bcast(buffer, count, datatype, root, comm);

  return ret;
  
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
  queue = malloc(sizeof(node));
  queue->next = NULL;
  queue->data = NULL;
  queue->type = FIRST_LINK;

  return;

}

static void add_link (node *n){
 
  n->next = queue;
  queue = n;
  fprintf(stderr,"the node added is now the head of the queue.  its type is %d\n", queue->type);

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
  struct stat sb;
  mode_t pmask;
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  itoa(rank, buf, 10);
  GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));  
/*   strcpy(file_name, cCurrentPath); */
/*   strcat(file_name, "/"); */
/*   strcat(file_name, buf); */
/*   strcat(file_name, txt); */  
  strcat(cCurrentPath, "/");
  strcat(cCurrentPath, out_file);
  strcpy(file_name, cCurrentPath);
  //strcat(file_name, "/");
  strcat(file_name, buf);
  strcat(file_name, txt);
  /* If the directory doesn't exist yet, make it.  Otherwise don't bother */
  if(stat(cCurrentPath,&sb)){
	//pmask = umask(0);
	dir_check = mkdir(cCurrentPath, S_IRWXU );
	//chmod(cCurrentPath, S_IRWXU | S_IROTH | S_IXOTH);
	dir_check = chmod(cCurrentPath, 0755);
	//if (dir_check == -1)
	  //fprintf(stderr, "chmod failed, errno = %d\n", errno);
	//umask(pmask);
	
  }/*  else{ */
/* 	//fprintf(stderr, "directory already exists.\n"); */
/*   } */
 
  return;

}


static int open_fp(void *fd){

  
  fd = fopen(file_name,"w");
  if (fd){
	//fprintf(stderr, "File pointer successfully opened\n");
    return 0;
  }
  else{
	//fprintf(stderr, "File pointer open failed.");
	return -1;
  }
}


static void pp_send(mpi_data_send *s){
  int i;  
  int count;
  MPI_Datatype datatype;
  int dest;
  int tag;
  MPI_Comm comm;
  void *fd;
  //open_fp(fd);
  fprintf(stderr,"%s\n", file_name);
  fd = fopen(file_name,"a");
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
  fprintf(fd, "SIZE: %d\n", s->size);
  fprintf(fd, "DEST: %d\n", s->dest);
  fprintf(fd, "START TIME: %f\n", s->fn_start_time);
  fprintf(fd, "END TIME: %f\n\n", s->fn_end_time);

  fclose(fd);
  return;
}


static void pp_recv(mpi_data_recv *s){
  int i;  
  int count;
  MPI_Datatype datatype;
  int dest;
  int tag;
  MPI_Comm comm;
  void *fd;

  //open_fp(fd);
  fd = fopen(file_name,"a");
  if (fd == NULL)
	fprintf(stderr,"pretty print of mpi_send data failed.\n");
  fprintf(fd, "TYPE: RECV\n");
  fprintf(fd, "COMM: %s\n", s->comm);
  fprintf(fd, "NODE: %d\n", s->rank);
  fprintf(fd, "ID: ");
  for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	fprintf(fd, "%02x", s->MD5[i]);
  fprintf(fd, "\n");
  fprintf(fd, "DTYPE: %s\n", s->datatype);
  fprintf(fd, "SIZE: %d\n", s->size);
  fprintf(fd, "SRC: %d\n", s->src);
  fprintf(fd, "START TIME: %f\n", s->fn_start_time); 
  fprintf(fd, "END TIME: %f\n\n", s->fn_end_time);

  fclose(fd);
  return;
}


static void print_q(){
  node *curr = queue;
  

/*   FIRST_LINK = -1, */
/*   SEND = 0, */
/*   RECV, */
/*   ISEND, */
/*   IRECV, */
/*   BCAST */
  while(curr){
	fprintf(stderr,"inside while\n");
	switch(curr->type){
	case FIRST_LINK:
	  {
	  break;
	  }
	case SEND:
	  {
	  pp_send(curr->data);
	  fprintf(stderr,"should have printed a SEND\n");
	  break;
	  }
	case RECV:
	  {
	  pp_recv(curr->data);
	  fprintf(stderr,"should have printed a RECV\n");
	  }
	default:
	  {
	  break;
	  }
	}
	curr = curr->next;
  }
  fprintf(stderr,"should have printed\n");

}

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
  /*
	WARNING:
	itoa modified to remove the string terminator to use with mkdir!
	don't use for other strings!!!
	-ASW
  */
  str[n] = '\0';
  
  for (p = str, q = p + (n-1); p < q; ++p, --q)
	c = *p, *p = *q, *q = c;
  return str;
}

static double get_time(){
  struct timeval t;
  gettimeofday(&t, NULL);
  double d = t.tv_sec + (double) t.tv_usec/1000000;
  return d;
}
