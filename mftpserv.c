//Ryan Slyter
//cs360
//final server
#include<stdlib.h>
#include<stdio.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<string.h>
#include<netdb.h>
#include<unistd.h>
#include<dirent.h>
#include<limits.h>
#include<sys/wait.h>
#include"mftp.h"

size_t result; //global result for read/write calls
char databuf[MAXIMUM_PATH]; //will hold file paths in data xfer funcs
int controllistenfd; //used for first connection
int control_fd; //main control descriptor
int tempfd; //global file descriptor used for data connections

/**BOOLEAN FLAGS**
  data_flag: Flipped on to used modified connect() to create data fd
  w_msg: Flipped on to send an acknowledgement with msg appended
  data_connection: Flipped on only if a data connection is established
 */
int data_flag = 0, w_msg = 0, data_connection = 0;
char newport[20]; //will hold new port # to be sent to cient

/*********************************************************************/
                         //UTILITY FUNCTIONS//
/*********************************************************************/

//helper function to keep reading from 1 file and writing to another
//until EOF detected from the write file
void custom_read(int from, int to){
  char temp[1];
  while ((result = read(from, temp, 1)) != 0){
    if ((result = write(to, temp, 1)) < 0){
      printf("File reading/writing failed.\n");
      printf("Fatal error. Program exit.\n");
      exit(1);
    }
  }
}

void get_path(){
  int i = 0;
  char c;  

  while (1){
    result = read(control_fd, &c, 1);
    if (c == '\n'){
      databuf[i] = '\0';
      break;
    }
    databuf[i] = c;
    i++;   
  }
}

//helper function to write an acknowledgement to the client
void acknowledge(){
  if (w_msg){
    w_msg--;
    printf("About to write custom acknowledgement.\n");

    if ((result = write(control_fd, "A", 1)) != 1){
      printf("Fatal error communicating with client. Program exit.\n");
      exit(1);
    }
    if ((result = write(control_fd, newport, strlen(newport))) < 0){
      printf("Fatal error communicating with client. Program exit.\n");
      exit(1);
    }    
    printf("Newport is %s----", newport);
    return;
  }  
  if ((result = write(control_fd, "A\n", 2)) != 2){
    printf("Fatal error communicating with client. Program exit.\n");
    exit(1);
  }
}

//helper function to write an error msg to the client
//NOTE**: Msg's MUST have a null term appended to them
void write_error(char* msg){
  if ((result = write(control_fd, "E", 1)) != 1){
    printf("Fatal error communicating with client. Program exit.\n");
    exit(1);
  }
  if ((result = write(control_fd, msg, strlen(msg))) < 0){
    printf("Fatal error communicating with client. Program exit.\n");
    exit(1);
  }
}

//main helper functions to have server set up the data connection for
//the client:
//NOTE: need to add an int arg here if data_c needs to do listen() also
void custom_connect(){
  struct sockaddr_in servaddr, clientaddr;
  struct sockaddr_in portaddr; //changed from saturday: sockaddr* portaddr
  int connectfd, listenfd; 
  int inlen = sizeof(struct sockaddr_in);
  
  //making the new socket
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    perror("Fatal error: socket call failed.\n");
    write_error("Could not create data connection.\n");
    exit(1);
  }

  //allocating and setting all the values of the socket
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  if (data_flag){
    servaddr.sin_port = htons(0); //0 wildcard for any port number
  }
  else{
    servaddr.sin_port = htons(MY_PORT_NUM);
  }
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0){
    perror("bind\n");
    write_error("Fatal error setting up data connection. Program exit.\n");
    exit(1);
  }

  //****** beginning of strictly data connection code ******//
  if (data_flag){
    data_flag--;
    memset(&portaddr, 0, sizeof(portaddr));    
    int addrlen = sizeof(struct sockaddr_in); //changed from saturday: (struct sockaddr*)
        
    if ((result = getsockname(listenfd, (struct sockaddr*) &portaddr, &addrlen)) < 0){
      write_error("Fatal error setting up data connection. Program exit.\n");
      exit(1);
    } 
    int port = ntohs(portaddr.sin_port);
    if ((result = sprintf(newport, "%d\n", port)) < 0){
      write_error("Could not convert port number for sending. Program exit.\n");
      return;
    }

    //printf("SERVER: sending port number %s", newport);
    w_msg++;
    acknowledge();
    listen(listenfd, 1);
    
    connectfd = accept(listenfd, (struct sockaddr*) &clientaddr, &inlen);
    tempfd = connectfd; //return new data connection fd to global tempfd
    return;
  } //****** end of strictly data connection code ******//

  //else the main local var just needed the listen fd from this funciton.
  controllistenfd = listenfd;

}

/*********************************************************************/
                               //MAIN//
/*********************************************************************/


int main(){
  //first 3 locals are just used to complete the control connection
  struct sockaddr_in clientaddr;
  int inlen = sizeof(struct sockaddr_in);  
  pid_t child; //id of the child process interfacing with the client

  custom_connect();

  listen(controllistenfd, 4);
  
  while(1){
    
    control_fd = accept(controllistenfd, (struct sockaddr*) &clientaddr, &inlen);    
    if ((child = fork()) == -1){
      printf("Internal server error forking off child process.\nProgram exit.\n");
      close(control_fd);
      exit(1);
    }
    
    if (!child){
      /****************** MAIN SERVER CODE **********************/
      while(1){

      server_command: ;

	char testchar[1];
	if ((result = read(control_fd, testchar, 1)) != 1){
	  printf("Nothing to read from control connection. Program exit.\n");
	  exit(1);
	}
 
	//actually testing to see which command this is now

	//** CASE "Q" **//
	if (!strcmp(testchar, "Q")){
	  read(control_fd, testchar, 1);
	  acknowledge();
	  close(control_fd);
	  exit(0);
	}
	//** END "Q" **//

	//** CASE "L" **//
	if (!strcmp(testchar, "L")){
	  read(control_fd, testchar, 1);

	  if (data_connection){
	    pid_t grandchild;
	    if ((grandchild = fork()) == -1){
	      write_error("Fatal internal server error.\n");
	      printf("Server fatal error forking grandchild.\n");
	      close(tempfd);
	      goto server_command;
	    }
	    if (!grandchild){
	      if (dup2(tempfd, 1) < 0){
		printf("Internal i/o redirection error.\n");
		exit(1);
	      }

	      printf("SERVER: made it to execlp\n");
	      execlp("/bin/ls", "ls", "-l", NULL);
	      printf("Fatal Error: execution of remote ls failed. Program exit.\n");
	      exit(1);	    
	    }
	    else{
	      wait(&grandchild); //wait for ls to finish
	      printf("SERVER: back from exelp\n");
	      close(tempfd); //close write end of file descriptor
	      data_connection--; //flip to data connect flag
	      acknowledge(); //send postivie acknowledgement to client
	      goto server_command;
	    }
	  }
	  else{
	    write_error("No Data Connection has been established yet.\n");
	    goto server_command;
	  }
	}
	//** END "L" **//

	//** CASE "G<PATH>" **//
	else if (!strcmp(testchar, "G")){

	  get_path();

	  if (data_connection){
	    int file;
	    if ((file = open(databuf, O_RDONLY)) < 0){
	      write_error("File path could not be opened.\n");
	      close(tempfd);
	      goto server_command;
	    }
	    custom_read(file, tempfd);
	    close(tempfd);
	    close(file);
	    data_connection--;
	    acknowledge();
	    goto server_command;	  
	  }
	  else {
	    write_error("No data connection has been set up yet.\n");
	    goto server_command;
	  }
	}
	//** END "G" **//

	//** CASE "P<PATH>" **//
	else if (!strcmp(testchar, "P")){

	  get_path();

	  if (data_connection){
	    int file;
	    if ((file = open(databuf, O_RDWR | O_CREAT | O_EXCL, 777)) < 0){
	      write_error("File path could not be opened.\n");
	      close(tempfd);
	      goto server_command;
	    }
	    acknowledge();
	    custom_read(tempfd, file);
	    close(tempfd);
	    close(file);
	    data_connection--;
	    // acknowledge();
	    goto server_command;
	  }
	  else {
	    write_error("No data connection has been set up yet.\n");
	    goto server_command;
	  }
	}
	//** END "P<PATH>" **//

	//**CASE "C<PATH>" **//
	else if (!strcmp(testchar, "C")){

	  get_path();
	  char* path = realpath(databuf, NULL);
	  if (path == NULL){
	    write_error("Directed path does not exist.\n");
	    goto server_command;
	  }
	  if (chdir(path) < 0){
	    write_error("Directed path could not be navigated.\n");
	    printf("Directed path could not be navigated.\n");
	    goto server_command;
	  }
	  acknowledge();
	  goto server_command;
	  
	}
	//** END "C<PATH>" **//

	//** CASE "D" **//
	else if (!strcmp(testchar, "D")){
	  read(control_fd, testchar, 1);
	  //clear_fd(control_fd);
	  data_flag++;
	  custom_connect();
	  data_connection++;
	  goto server_command;
	}
	//** END "D" **//

      }
      /*************** END OF MAIN SERVER CODE ******************/
    }

    //PARENT CODE
    else {
      wait(&child); //child received exit msg from client
      if (!child){
	close(control_fd);
      }
      else if (child < 0){
	write_error("Server Child process did not exit properly.\n");
	close(control_fd);
	exit(1);
      }
    }

  }
  /* should never get to this point because server is just acting
     like a daemon. Needs to be killed with control+c */
  return 0;
}
