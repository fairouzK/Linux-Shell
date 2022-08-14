#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#define _GNU_SOURCE
#define sep_char " "
#define HISTORY_SIZE 10
#define MAX_LINE 80
	

struct backGdCommands {
    int count;
    char *command[1024]; 
    int processID[1024];
};

struct historyStrct {
    int count;
    char *cmd[1024];
};


int noOfArgs; // variable to count the number of commands
int cmdLen; // variable to store command length
char inputFile[1024];
char outputFile[1024];
int posInF; int posOutF;
int inBg=0;
int getInpFromFile; 
int printToFile;
char *tokens[1024];

char *getcwd(char *buf, size_t size);//show the path Of the current file
void printPrompt();//print the prompt
void readCommandLine(char* input,struct historyStrct *history);
int isBuiltInCommand(char* command);
void cd_func();
void kill_func(struct backGdCommands *bC);
void jobs_func(struct backGdCommands *bC);
void history(struct  historyStrct *hStruct);
void store_BgJob(char *command, struct backGdCommands *bC);
void garbageCollector();
void store_command(char *command, struct historyStrct *history);
void remove_bGJob(int num,struct backGdCommands *bC);
int index_Check(char* comm,struct historyStrct *history);
void redDirecting();
void dup_Func();

int main (int argc, char **argv)
{  
   
   struct backGdCommands bC; 
   bC.count=0;

   struct  historyStrct hSt; 
   hSt.count=0; 
     
   int i=0; //to count the number of background jobs
   char cmdLine[1024];
   char* cmd;
   char input[1024]; 
   int standardOut=0;
   int standardIn=0;
   int outputFileDescriptor;
   int inputFileDescriptor;
   int bG_counter=1; 
   while (1)
   { 
       
        int status;
        printPrompt();
        readCommandLine(cmdLine,&hSt); /* read command line*/
        while(strlen(cmdLine)==0) 
        { 
            printPrompt(); 
            readCommandLine(cmdLine,&hSt);        
        }
        
        if(!inBg)
            redDirecting();
    
        store_command(cmdLine,&hSt); //cmdLIne is the first token
        
       if(strcmp(tokens[0],"$PATH")==0)
        {
            const char* s=getenv("PATH");
            printf("%s\n",(s!=NULL)?s:"getenv returned NULL");                                    
        }

       else if(isBuiltInCommand(tokens[0]))
        {            
            if(strcmp("cd",tokens[0])==0) //if the first entered command is "cd"
               {
                    
                    redDirecting();
                    if(printToFile==1 )
                    {
                        for(int i=posOutF;i < noOfArgs;i++)
                            tokens[i]='\0';  
                        outputFileDescriptor = open(outputFile,O_CREAT | O_RDWR, 0666);              
                        if(outputFileDescriptor==-1)
                            printf("stderr, failed output redirect from %s\n",outputFile);
                        else
                        {
                            standardOut = dup(STDOUT_FILENO); 
                            dup2(outputFileDescriptor, 1);
                            close(outputFileDescriptor); 
                            cd_func();  
                            dup2(standardOut, STDOUT_FILENO);				
                        }    
                        
                    }   
                     if(getInpFromFile==1)
                    {
                         for(int i=posInF;i < noOfArgs;i++)
                            tokens[i]='\0';  
                        inputFileDescriptor = open(inputFile,O_RDONLY);              
                        if(inputFileDescriptor==-1)
                            printf("stderr, failed input redirect from %s\n",inputFile);
                        else
                        {
                            standardIn = dup(STDIN_FILENO); 
                            dup2(inputFileDescriptor, 0);
                            close(inputFileDescriptor); 
                            cd_func();  
                            dup2(standardIn, 0);				
                        }       
                    }      
                    else
                              cd_func();                                  
               } 

            else if(strcmp("exit",tokens[0])==0) //if the first entered command is "exit" 
                 {
                    if(bC.count!=0)
                    {
                        printf("background jobs running currently:%d\n",bC.count);
                        printf("To exit, please kill every background job\n");   
                    }
                    else 
                        exit(0);
                }

            else if(strcmp("jobs",tokens[0])==0) //if the first entered command is "jobs"               
                {               
                    redDirecting();
                    if(printToFile==1 )
                    {
                        outputFileDescriptor = open(outputFile,O_CREAT | O_RDWR, 0666);              
                        if(outputFileDescriptor==-1)
                            printf("stderr, failed output redirect from %s\n",outputFile);
                        else
                        {
                            standardOut = dup(STDOUT_FILENO); 
                            dup2(outputFileDescriptor, 1);
                            close(outputFileDescriptor); 
                            jobs_func(&bC); 
                            dup2(standardOut, STDOUT_FILENO);				
                        }      
                    }                         
                    else
                            jobs_func(&bC);                                  
                } 

            else if(strcmp("kill",tokens[0])==0) //if the first entered command is "kill" 
                    kill_func(&bC);
                    
            else if(strcmp("history",tokens[0])==0) //if the first entered command is "history"                 
                {   
                    redDirecting();
                    if(printToFile==1 )
                    {
                        outputFileDescriptor = open(outputFile,O_CREAT | O_RDWR, 0666);              
                        if(outputFileDescriptor==-1)
                            printf("stderr, failed output redirect from %s\n",outputFile);
                        else
                        {
                            standardOut = dup(STDOUT_FILENO); 
                            dup2(outputFileDescriptor, 1);
                            close(outputFileDescriptor); 
                             history(&hSt);
                            dup2(standardOut, STDOUT_FILENO);				
                        }      
                    }  
                    else
                             history(&hSt);
                }                                                                                         
        }
    
        else  //if the command is not built-in
        {
            int childPid=fork(); 
            if(childPid<0) 
                {
                    printf("fork failed\n");
                    exit(0);
                }
            else if(childPid==0)
            {                                
                dup_Func();
                if(execvp(tokens[0],tokens)==(-1))
                {  
                    printf("Error executing command\n");
                    exit(0); 
                }    
            }
            else
            {
                 
                if(inBg==1)
                { 
                    //record to the list of background jobs
                    cmdLine[cmdLen - 1]=0;
                    store_BgJob(cmdLine,&bC);
                    bC.processID[i]=childPid;
                    i++;
                    bC.count=i;     
                    printf("[%d] %d\n",bC.count,childPid); 
                    inBg=0;
                }
                
                else
                {
                    pid_t wpid=waitpid(childPid,&status,WUNTRACED);
                    if (wpid == -1) 
                    {
                        perror("waitpid");
                        exit(EXIT_FAILURE);
                    }    
                }
            }
        } 
           
    for(int i=0;i < noOfArgs;i++)
        tokens[i]='\0';  
    }
    return 0;
}

//**********************************************************************************
void printPrompt()
{ 
    char *rPtr;
    char *ptr;

    long size = pathconf(".", _PC_PATH_MAX); //size of the path of the file

    if ((ptr = (char *)malloc((size_t)size)) != NULL)
    rPtr= getcwd(ptr, (size_t)size); 
    printf("%s%s ",rPtr,"%"); 
    free(ptr);
}

//**********************************************************************************
void readCommandLine(char cmd[],struct historyStrct *history)
{
    char* command;
    int i=0;
    char* input; 
    char* in;
    if((in=fgets(input,MAX_LINE,stdin))==0)
    {
        printf(" ");
    }
    else
    {
      long strlength=strlen(input);
      if (strlength > 0)  //replacing the last new line charatcter that fgets return along with the input read
        if (input[strlength - 1] == '\n') input[strlength - 1] = '\0';
    }

    if(index_Check(input,history)==0)
      strcpy(cmd,input); 

    cmdLen=strlen(input); 
    
    //breaking the command into tokens and storing them in an array of pointers
    command=strtok(input,sep_char);
    while(command!= NULL)
    {
        tokens[i]=(char*)malloc((sizeof(char)+1)*strlen(command)); //allocate memory for the first token
        strcpy(tokens[i],command);  
        command=strtok(NULL,sep_char); 
        i++; 
    }
    noOfArgs=i;   

    if(cmdLen > 1 && strcmp(tokens[noOfArgs - 1], "&") == 0 )
        {  
            tokens[noOfArgs - 1]='\0'; 
            noOfArgs-=1;  
            inBg=1; 
        }          
}
//*****************************************************************************************************
int isBuiltInCommand(char* command){
if((strcmp("cd",command)==0) || (strcmp(command,"jobs")==0) || (strcmp(command,"kill")==0) || (strcmp(command,"exit")==0) || (strcmp(command,"history")==0))
    {
        return 1; //1=true 
    }
return 0; //0=false
}
//*****************************************************************************************************
void cd_func()
{
		char* path=tokens[1];   
        if (tokens[1] == NULL ||  tokens[1][0] == '~' && ((strlen(tokens[1]) == 1) || (strlen(tokens[1]) == 2 && tokens[1][1] == '/'))) { // go back to home
        if((path = (char *) malloc(1024))!=NULL); 
            strcpy(path, getenv("HOME"));
        }
    // the .. and / and a given directory are directly executed here        
        if(chdir(path)==(-1) )
        printf("Error changing directory\n");				
}
//********************************************************************************************
void jobs_func(struct backGdCommands *bC)
{ 
    int order=1;
    if(bC->count==0)
    printf("No background jobs\n");
    else
    { 
        for(int j=0;j<=bC->count;j++)
        {                 
            int status; 
            pid_t return_pid = waitpid(bC->processID[j], &status, WNOHANG); 
            if (return_pid ==0)
            {
                /* child is still running */
                printf("[%d]\t\t%s\n",order++,bC->command[j]);
            } 
        }                        
    }   
}
//*****************************************************************************************************
void history(struct  historyStrct *hStruct)
{  
    if(hStruct->count==0)
        printf("No history\n");
    else
    {   int j;
        if(hStruct->count>5)        
            j=hStruct->count-5;
        else 
            j=0;
        for (int i = j+1;i <=hStruct->count; i++)   
        printf("%d %s\n", i, hStruct->cmd[i-1]);
    } 
}
//***************************************************************************************
void store_command(char *command, struct historyStrct *history) {
 
        history->cmd[history->count] = (char *) malloc(MAX_LINE * sizeof(MAX_LINE));
        strcpy(history->cmd[history->count], command);
        history->count++;
}
//*******************************************************************************************
void store_BgJob(char *command, struct backGdCommands *bC) {
 
        bC->command[bC->count] = (char *) malloc(MAX_LINE * sizeof(MAX_LINE));
        strcpy(bC->command[bC->count], command);
}
//*******************************************************************************************
void kill_func(struct backGdCommands *bC)
{
    char* s=tokens[noOfArgs - 1];
    int i=tokens[noOfArgs - 1][1]-'\0'; 
    int x=0;
    int counter=1;
    if(s[0]== '%'&& noOfArgs==2  && isdigit(i) && bC->count>0 )
    {
        while(isdigit(i) && counter < strlen(tokens[noOfArgs - 1]))
        {
            i=i-48;  //subtract an ascii of 48 to get the integer value
            x=x*10+i;    //retrieve the number after '%'
            counter++;
            i=tokens[noOfArgs - 1][counter]-'\0';   //gives the ascii value of the digit         
        }

        if(i=='\0')
        {
            if(i<=bC->count)
                kill(bC->processID[x-1],SIGKILL);
                remove_bGJob(x-1,bC);                
        }
        else
            printf("Bad parameter\n"); 
    }  
    else if(bC->count==0)
        printf("No background job to kill\n"); 
    else
        printf("Command not recognized\n"); 
}
//*********************************************************************************************************
void remove_bGJob(int num,struct backGdCommands *bC){
    for(int j=num;j< bC->count;j++) 
           bC->command[j]=bC->command[j+1];
             bC->count--;
           //  printf("COMES: %d\n", bC->count);
}
//*****************************************************************************************************************
int index_Check(char* comm,struct historyStrct *history)
{    
    if (comm[0] == '!') 
    { 
        if (noOfArgs > 1) 
        {
            printf("Only 1 parameter expected!.\n");
            return 1;
        }
        if (comm[1] == '-' && isdigit(comm[2])) //repeat the last command (!-1)
        { 
            int a=0; 
            int counter=2;        
            int y=comm[2]; 
            while(isdigit(y) && counter < strlen(comm))
            {                
                y=y-48;
                a=a*10+y;    //retrieve the number after '%'
                counter++;
                y=comm[counter];
            }

            if(y=='\0')
                strcpy(comm,history->cmd[history->count-a]);                 
        }
        else if(isdigit(comm[1])) //check for !num
        {   
            int x=0; 
            int counter=1;        
            int y=comm[1]; 
            while(isdigit(y) && counter < strlen(comm))
            {                
                y=y-48;
                x=x*10+y;    //retrieve the number after '%'
                counter++;
                y=comm[counter];
            }

            if(y=='\0')
                strcpy(comm,history->cmd[x-1]); 
            else if (strcmp(comm, "!") == 0) 
            {
                printf("Event not found.\n");
                return 1;
            }            
            else if (x <= 0 || x > history->count) 
            {
                printf("No such command in history.\n");
                return 1;
            }                                  
        } 
        else
            printf("Bad command\n");      
    }
return 0; 
}
//**************************************************************************************************
void redDirecting()
{
    getInpFromFile=0; 
    printToFile=0; 
    for(int i=0;i < noOfArgs;i++)
    {
        if (strcmp(tokens[i],">")==0)  //redirecting to an output file
        { 
            if (tokens[i+1] == NULL )
            {
					printf("Not enough input arguments\n");
            }
            else
            {   
                printToFile=1; 
                strcpy(outputFile,tokens[i+1]); 
                posOutF=i;
            }           
        }
        if(strcmp(tokens[i],"<")==0) //getting input from a file
        {
            if (tokens[i+1] == NULL )
            {
					printf("Not enough input arguments\n");
            }
            else
            {   
                getInpFromFile=1;
                strcpy(inputFile,tokens[i+1]);
                posInF=i; 
            }      
        }
    }
}
//***********************************************************************************************************
void dup_Func()
{

if (printToFile==1)
    {
        int outputFileDescriptor = open(outputFile,O_CREAT | O_RDWR, 0666);              
        if(outputFileDescriptor==-1)
            printf("stderr, failed output redirect from %s\n",outputFile);
        else
        {
            dup2(outputFileDescriptor, 1);
            close(outputFileDescriptor); 
            for(int i=posOutF;i < noOfArgs;i++)
            tokens[i]='\0';  
        }                      
    }
    if (getInpFromFile==1)
    {
        int inputFileDescriptor = open(inputFile, O_RDONLY); // open the file
        if(inputFileDescriptor==-1)
            printf("stderr, failed input redirect from %s\n",inputFile);
        else
        {
            dup2(inputFileDescriptor,0);
            close(inputFileDescriptor);
            for(int i=posInF;i < noOfArgs;i++)
            tokens[i]='\0';
        }
    }


}



