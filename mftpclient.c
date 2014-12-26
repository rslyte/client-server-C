//Ryan Slyter
//cs360
//client server
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
char readbuf[1]; //global buffer used for reading
int newport; //global used for data connections
int tempfd; //global file descriptor used for data connections
int control_fd; //main control descriptor
char inputbuf[MAX_BUFF]; //local to main buffer for user inputs
char* hostname;

/************************************************************/
                          //SUPPORT FUNCTIONS//
/************************************************************/

//helper function to keep reading from 1 file and writing to another
//until EOF detected from the write file
void custom_read(int from, int to){
  while ((result = read(from, readbuf, 1)) != 0){
    if ((result = write(to, readbuf, 1)) < 0){
      printf("File reading/writing failed.\n");
      printf("Fatal error. Program exit.\n");
      exit(1);
    }
  }
}

void rest_of_msg(int fd){
  while (result = read(fd, readbuf, 1)){ //just making sure we clear out the fd  
    printf("%s", readbuf);
    if (!strcmp(readbuf, "\n"))break;
  }
}

//helper function to automatically print and return server acknowledge boolean
//useful for commands which only read an A or an E from the server
int server_msg(int fd){
  if ((result = read(fd, readbuf, 1)) != 1){
  printf("No response from server!\n");
  return 0;
  }
  printf("Server Response: %s", readbuf);
  if (!strcmp(readbuf, "A")){
    rest_of_msg(fd);
    return 1;
  }
  else if (!strcmp(readbuf, "E")){
    printf("\nError Message: ");
    rest_of_msg(fd);
    return 0;
  }
  printf("Server countered with invalid response (no 'A' or 'E'):\n");
  rest_of_msg(fd);
  return 0;
}

//Helper function which is c/p'd code from Assignment (9?) to connect to socket
int serv_connect(char* hostname, int portnum){

   int socketfd;
   struct sockaddr_in servaddr;
   struct hostent* hostEntry;
   struct in_addr **pptr;

   if((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
      printf("socket fail from client side\n");
      exit(1);
   }

   memset(&servaddr, 0, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons(portnum);
  
   hostEntry = gethostbyname(hostname); 
  
   if (!hostEntry){
      herror("Error obtaining hostname, program exit.\n");
      exit(1);
   }
   
   pptr = (struct in_addr **) hostEntry->h_addr_list;
   memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));

   if(connect(socketfd, (struct sockaddr *) &servaddr, sizeof(servaddr))){
      perror("Connection to host failed (socketfd)\n");
      exit(1);
   }   
   return socketfd;
}

//NOTE: I make errors in setting up the data connection fatal. What's the point of continuing
//the program on these errors if half the client commands can't be executed because this failed?
void get_data_connect(){
  
  char databuf[MAX_BUFF];
  if ((result = write(control_fd, "D\n", 2)) != 2){
    printf("Error writing data connection command to host server.\n");
    printf("Fatal error. Exiting.\n");
    exit(1);
  }
       
  if ((result = read(control_fd, readbuf, 1)) != 1){
    printf("Error reading from server acknowledgement. Fatal error. Exiting.\n");
    exit(1);
  }
  if (strcmp(readbuf, "A")){
    printf("Server responded with 'E': Program fatal error. Exiting\n");
    exit(1);	   
  }       
  //trying to read the new port number
  int i = 0;
  while ((result = read(control_fd, &databuf[i], 1)) && databuf[i] != '\n'){
    i++;
  }
  databuf[i] == '\0'; //get rid of newline so that it's just a string
  newport = atoi(databuf);
  printf("Client got new port number %d to connect to.\n", newport);
     
  tempfd = serv_connect(hostname, newport); //error checking within function
  printf("Client now has data connect fd %d\n", tempfd);

  return;
}

char* get_filename(char* str){
  char* parsed = strtok(str, "/");
  char* temp;
 
  for (temp = parsed; temp != NULL; temp = strtok(NULL, "/")){
    if (temp[strlen(temp) - 1] == '\n'){
      temp[strlen(temp) - 1] == '\0';
      return strdup(temp);
    }
  } 
  return NULL; //need to make sure to free this when done
}

/***********************************************************/
                                  //MAIN//
/***********************************************************/

int main(int argc, char* argv[]){
  
  //   char* parsed; //local to main var holds the next string token during processing
     
   if (argc != 2){
      printf("Usage: ./daytime (host address or name)\n");
      exit(1);
   }
   hostname = argv[1];

   control_fd = serv_connect(hostname, MY_PORT_NUM); //function call to create control fd

   printf("Connection to host %s succeeded, network commands can now be executed...\n", argv[1]);
   printf("User commands:\nexit\ncd <pathname>\nls\nget <pathname>\nshow <pathname>\nput <pathname>\n");

   /*************MAIN EXECUTION LOOP****************/

   while (1){ //while loop to keep getting commands from the user
     //errors from server or invalid input will goto this point
   skip_to_command: 
   
     // parsed = NULL;

     printf("Please print a command to be sent to the server:\n");
  
     if (fgets(inputbuf, sizeof(inputbuf), stdin) == NULL){
       printf("Error getting command from the user, please enter a different commmand.\n");
       goto skip_to_command; //might want to change this to exit          
     }

     printf("YOU ENTERED COMMAND : %s\n", inputbuf);
     char* parsed = strtok(inputbuf, " "); //NOTE: tokens have a newline appended automatically

     /******************************************************/

     //CASE: "EXIT" -QUIT THE SERVER/CLIENT PROGRAMS
     if (!strcmp(parsed, "exit\n")){
	
       if ((result = write(control_fd, "Q\n", 2)) != 2){
	 printf("Error writing exit command to host server\n");
	 goto skip_to_command;
       }
       if (server_msg(control_fd)){
	 printf("Exit command executed properly.\n");
	 break;
       }
       else{
	 printf("reverting back to main command sequence.\n");
	 goto skip_to_command;
       }
     }               
     //END CASE "EXIT"
     
     //CASE: "RCD <PATHNAME> -SERVER CHILD CHANGES DIR
     if (!strcmp(parsed, "rcd\n") || !strcmp(parsed, "cd\n")){
       printf("Command acknowledged but no required pathname given. Reverting back to main comand sequence.\n");
       goto skip_to_command;
     }
     else if (!strcmp(parsed, "rcd")){
       parsed = strtok(NULL, " "); //get the pathname
  
       char tosend[strlen(parsed) + 1];
       tosend[0] = 'C'; tosend[1] = '\0';
       strncat(tosend, parsed, strlen(parsed));
    
       if ((result = write(control_fd, tosend, strlen(tosend))) < 0){
	 printf("Error communicating with server, reverting back to main command sequence.\n");
       }	  
       if (server_msg(control_fd))goto skip_to_command;
       else{
	 printf("reverting back to main command sequence.\n");
	 goto skip_to_command;
       }	
     }
     //END CASE "RCD <PATHNAME>"
     
     //CASE: "CD <PATHNAME> -LOCALLY CHANGE DIR
     else if (!strcmp(parsed, "cd")){
       parsed = strtok(NULL, " ");
       if (!strcmp(parsed, ".\n")){
	 printf("Already in working in directory specified.\n");
	 goto skip_to_command;
       }
       if (!strcmp(parsed, "..\n")){
	 if (chdir("..") < 0){
	   printf("Change to parent directory failed or already at root dir. Reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }
	 printf("Change to parent directory successful. Reverting back to main command sequence.\n");
	 goto skip_to_command;
       }
       printf("Attempting to change to directory %s", parsed);
       parsed[strlen(parsed)-1] = '\0'; //replace newline char with a null term
       char* path = realpath(parsed, NULL);
       if (realpath == NULL){
	 printf("Error resolving directory path, reverting back to main command sequence\n");
	 goto skip_to_command;
       }
       //printf("DEBUG: after realpath returned %s\n", path);
       if (chdir(path) < 0){
	 printf("%s is not a directory, there were too many symbolic links in path, or a component of the path required permissions that could not be resolved. Reverting back to main command sequence.\n", parsed);
	 free(path);
	 goto skip_to_command;
       }
       else{
	 free(path);
	 printf("Working local directory is now %s.\n", path);
	 goto skip_to_command;
       }
     }
     //END CASE "CD <PATHNAME>"

     //CASE "LS" -CLIENT DISPLAYS ALL FILES IN THE DIRECTOR
     else if (!strcmp(parsed, "ls\n")){
       pid_t child;
       if ((child = fork()) == -1){
	 printf("Internal Error executing command. Program will exit.\n");
	 exit(1);
       }
       if (!child){
	 if (system("ls -l | more -20") < 0){
	   exit(1);
	 }
	 exit(0);
       }
       else{
	 wait(&child);
	 if (child == 0){
	   printf("Command executed succesfully, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }
	 else{
	   printf("Command could not be executed, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }
       }
     }
     //END CASE "LS"

     //CASE "RLS" -CLIENTS TELL SERVER TO LIST FILES IN DIRECTORY
     else if (!strcmp(parsed, "rls\n")){

       get_data_connect();
       
       //data connection now established, can fork a child etc.
       //quering the server to list dir contents on the file descriptor
       if ((result = write(control_fd, "L\n", 2)) != 2){
	 printf("Error asking server to list its current directory contents.\n");
	 exit(1);
       }

       if (!server_msg(control_fd)){
	 printf("Reverting back to main command sequence.\n");
	 goto skip_to_command;
       }
              
       pid_t child;
       if ((child = fork()) == -1){
	 printf("Internal Error executing command. Program will exit.\n");
	 close(tempfd);//making sure no open connection after error exiting
	 close(control_fd);
	 exit(1);
       }

       if (!child){  //main child code	 
	 	
	 if (dup2(tempfd, 0) < 0){
	   printf("Internal i/o redirection error.\n");
	   exit(1);
	 }
	 	 	 
 	 printf("Child Msg: about to execlp.\n");
	 execlp("/bin/more", "more", "-20", NULL);
	 printf("Fatal Error: execution of remote ls failed. Program exit.\n");
	 exit(1);
       }
       else{
	 if (close(tempfd) < 0){
	   printf("Could not close temporary file descriptor after execution.\n");
	 }
	 wait(&child);
	 if (child == 0){
	   printf("Command executed succesfully, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }
	 else{
	   printf("Command could not be executed, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }	 
       }  
                   
     }
     //END CASE "RLS"

     else if (!strcmp(parsed, "get\n")){
	 printf("Pathname of file needs to be specified, reverting back to main command sequence.\n");
	 goto skip_to_command;
     }

     //CASE "GET<PATHNAME>" - GET CONTENTS OF FILE AND STORE IT LOCALLY
     else if (!strcmp(parsed, "get")){
       
       parsed = strtok(NULL, " "); //get the pathname

       get_data_connect();
    
       char tosend[strlen(parsed) + 1];
       tosend[0] = 'G'; tosend[1] = '\0';
       strncat(tosend, parsed, strlen(parsed));
    
       if ((result = write(control_fd, tosend, strlen(tosend))) < 0){ //write to control and make sure its there
	 printf("Error communicating with server, reverting back to main command sequence.\n");
	 goto skip_to_command;
       }	  
       
       if (!server_msg(control_fd)){
	 printf("%s does not exist remotely or is not a regular file.\nReverting back to main command sequene.\n", tosend);
	 if (close(tempfd) < 0){
	   printf("Could not close extraneous connection. Fatal error. Program exit.\n");
	   exit(1);
	 }
	 goto skip_to_command;
       }	
       //code to actually read from this now.

       parsed[strlen(parsed) - 1] = '\0';
     
       int newfd;
      
       if ((newfd = open(parsed, O_RDWR | O_CREAT | O_EXCL)) < 0){
	 printf("File could not be created, reverting back to main command sequence.\n");
	 close(tempfd);
	 goto skip_to_command;
       }

       custom_read(tempfd, newfd);
       printf("Command executed successfully. Reverting back to main command sequence.\n");
       close(tempfd);
       close(newfd);
       goto skip_to_command;
       
     }
     //END CASE "GET<PATHNAME>"
     
     else if (!strcmp(parsed, "show\n")){
       printf("Pathname of file needs to be specified, reverting back to main command sequence.\n");
       goto skip_to_command;
     }

     //CASE "SHOW<PATHNAME>" - GET CONTENTS OF FILE AND DISPLAY IT TO USER
     else if (!strcmp(parsed, "show")){
       parsed = strtok(NULL, " "); //get the pathname

       get_data_connect();

       char tosend[strlen(parsed) + 1];
       tosend[0] = 'G'; tosend[1] = '\0';
       strncat(tosend, parsed, strlen(parsed));
    
       if ((result = write(control_fd, tosend, strlen(tosend))) < 0){ //write to control and make sure its there
	 printf("Error communicating with server, reverting back to main command sequence.\n");
	 goto skip_to_command;
       }	  
       
       if (!server_msg(control_fd)){
	 printf("%s does not exist remotely or is not a regular file.\nReverting back to main command sequene.\n", tosend);
	 if (close(tempfd) < 0){
	   printf("Could not close extraneous connection. Fatal error. Program exit.\n");
	   exit(1);
	 }
	 goto skip_to_command;
       }

       //now fork the child and call show on this file descriptor...
       pid_t child;
       if ((child = fork()) == -1){
	 printf("Internal Error executing command. Program will exit.\n");
	 close(tempfd);//making sure no open connection after error exiting
	 close(control_fd);
	 exit(1);
       }

       if (!child){  //main child code	 
	 	
	 if (dup2(tempfd, 0) < 0){
	   printf("Internal i/o redirection error.\n");
	   exit(1);
	 }
	 	 	 
 	 printf("Child Msg: about to execlp.\n");
	 execlp("/bin/more", "more", "-20", NULL);
	 printf("Fatal Error: execution of remote ls failed. Program exit.\n");
	 exit(1);
       }
       else{
	 if (close(tempfd) < 0){
	   printf("Could not close temporary file descriptor after execution.\n");
	 }
	 wait(&child);
	 if (child == 0){
	   printf("Command executed succesfully, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }
	 else{
	   printf("Command could not be executed, reverting back to main command sequence.\n");
	   goto skip_to_command;
	 }	 
       }
    
     }
     //END CASE "SHOW<PATHNAME>"
     
     else if (!strcmp(parsed, "put\n")){
       printf("Pathname of file was not specified, reverting back to main command sequence.\n");
       goto skip_to_command;
     }

     //CASE "PUT<PATHNAME>" - GET CONTENTS OF FILE AND DISPLAY IT TO USER
     else if (!strcmp(parsed, "put")){
       
       get_data_connect();

       parsed = strtok(NULL, " "); //get the pathname
       parsed[strlen(parsed) - 1] = '\0';

       int newfd;

       if ((newfd = open(parsed, O_RDONLY)) < 0){
	 printf("File could not be created, reverting back to main command sequence.\n");
	 close(tempfd);
	 goto skip_to_command;
       }

       custom_read(newfd, tempfd);

       parsed[strlen(parsed)] = '\n'; //because we have to replace the null term with newline

       char tosend[strlen(parsed) + 1];
       tosend[0] = 'P'; tosend[1] = '\0';
       strncat(tosend, parsed, strlen(parsed));
    
       if ((result = write(control_fd, tosend, strlen(tosend))) < 0){ //write to control and make sure its there
	 printf("Error communicating with server, reverting back to main command sequence.\n");
	 goto skip_to_command;
       }	  
       
       if (!server_msg(control_fd)){
	 printf("%s file contents could not be copied remotely.\nReverting back to main command sequene.\n", tosend);
	 if (close(tempfd) < 0){
	   printf("Could not close extraneous connection. Fatal error. Program exit.\n");
	   exit(1);
	 }
	 goto skip_to_command;
       }

       printf("Command executed successfully. Reverting back to main command sequence.\n");
       close(tempfd);
       close(newfd);
       goto skip_to_command;

     }
     //END CASE "PUT<PATHNAME>"

     else{
       printf("Invalid Command. Reverting back to main command sequence.\n");
       goto skip_to_command;
     }
   }//while(1) ending bracket
   
   if (close(control_fd) != 0){
     printf("Error close main connection, Program did not shut down properly\n");
     exit(1);
   }
printf("Client Program exit...\n");
return 0;
}	
