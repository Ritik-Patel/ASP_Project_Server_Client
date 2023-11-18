/*
Section 1
Submitted by: Ritik Patel, Gunjan Kumar Kalathia
ID: 110100962, 110094776
*/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ftw.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fnmatch.h>

#define SRV_HM_DIR getenv("HOME")
#define SRV_CONN 'S'
#define MRR_CONN 'M'
#define TEXT_RESP 1
#define STRUCT_RESP 2
#define S_PRT 8000
#define RESP_LEN_LIMIT 4096
#define MRR_IP "127.0.0.1"
#define MRR_PRT 8001
#define TAR_GZ_TEMPORARY "temporary.tar.gz"
#define ARGS_LIMIT 7
#define FLLIST_TEMPORARY "serv_templist_of_files.txt"
#define EXT_CNT_LIMIT 6
#define BUFF_LEN_LIMIT 4096
#define FILE_RESP 3

// Structure created Addressinfo given to IP of mirror along with port number 
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    int port_number;
} AddressInfo; 

// Functions to handle all commands that user will input and thier helper functions
int op_selected_getdirf(int skt_cli, char** usr_given_agrs);
void op_selected_filesrch(int skt_cli, char** usr_given_agrs);
bool findFileInfo(const char* path_fl, char* obtainedreply);
int execTarCmd(const char *final_file, const char *temp_file_list);
void cleanupTempFiles();
int transmitTarFile(int sktfd_cli, const char *path_fl);
int op_selected_tarfgetz(int skt_cli, char** usr_given_agrs);
long int obtainFileSize(const char *path_fl);
int op_selected_targzf(int skt_cli, char** usr_given_agrs, int cnt_for_exten);
bool lookForFile(const char* name_fl, char* path_to_look_at);
int op_selected_fgetss(int skt_cli, char** usr_given_agrs, int len_of_agr);

int transmitFile(int sktfd_cli, const char *path_fl);
// transfer text & file to client 
int transmitFileReply(int skt_cli, const char* name_fl);
int transmitTextReply( int skt_cli, char* serv_buf);
// Procedure for routing client to mirror
int sendClientToMirror(int skt_cli);
// Procedures for conducting recursive search on given usr_given_agrs 
int lookForName(char *dir_nm, char **nms_of_fl, int obtainedFlNmCnt, int *obtainedFlCnt);
int lookForSize(char *dir_nm, int usr_giv_sz1, int usr_giv_sz2, int *obtainedFlCnt);
int lookForDate(char *tilde_path, time_t dn1, time_t dn2, int *obtainedFlCnt);
int lookForExt(char *dir_nm, char **type_of_fl, int cnt_for_exten, int *obtainedFlCnt);
// eradicate line breaks 
void eradicatebrkLine(char serv_buf[]);
//Manage client instruction
void processClient(int skt_cli);

// convert time to system time format
time_t transformDateIntoEnvType(const char *str_for_tm, int valid_day_format);
void adjustTime(struct tm *strct_for_tm, int valid_day_format);
int parseDateString(const char *str_for_tm, struct tm *strct_for_tm);
// Decide server to establish a connection with: either server or mirror 
char sendClientToServOrMirror(int occurance_of_client);

// Give number to client defined as global variable 
int cli_id = 0;

// Main Function
int main(int argc, char *argv[])
{
    int sktd, clisktd, ptnm, prc_st;
    socklen_t len;
    struct sockaddr_in servAdd; //ipv4
    
    // Establish Socket
    if ((sktd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Err: can not create socket\n");
        exit(1);
    }

    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);    
    servAdd.sin_port = htons(S_PRT);

    //bind & listen socket 
    bind(sktd, (struct sockaddr *) &servAdd, sizeof(servAdd));
    listen(sktd, 5);
    printf("Active on port: %d\n", S_PRT);
    while (1)
    {   
        // Increment client number 
        cli_id++;
        
        // Determine which server to connect:server or mirror
        char where_to_conn = sendClientToServOrMirror(cli_id);
        
        // Connect client to server using accept func
        if(where_to_conn == SRV_CONN){
            clisktd = accept(sktd, (struct sockaddr *) NULL, NULL);
            printf("Client number: %d has connected\n", cli_id);
            
            // child proces will redirect client 
            if (!fork()){    
                processClient(clisktd);
                close(clisktd);
                exit(0);
            }
            waitpid(0, &prc_st, WNOHANG);
        }
        
        // Connet client with mirror
        else if(where_to_conn == MRR_CONN) {
            clisktd = accept(sktd, (struct sockaddr *) NULL, NULL);
            printf("Server can not take enteries currently, redirecting the client number: %d\n", cli_id);
            
            if (!fork()){   
                sendClientToMirror(clisktd);
                close(clisktd);
                exit(0);
            }
            waitpid(0, &prc_st, WNOHANG);
        }
    }
}   

// make a function which will process the commands entered by the useer
void processCommand(int sktfd_cli, const char *buf_for_cmd) {
    char cmd[BUFF_LEN_LIMIT];
    char *usr_given_agrs[ARGS_LIMIT];
    int usr_agrs_cnt = 0;

    // save copy of commands in a serv_buf to avoid hampering the orginal one
    char dup_buf[BUFF_LEN_LIMIT];
    strncpy(dup_buf, buf_for_cmd, sizeof(dup_buf));
    dup_buf[sizeof(dup_buf) - 1] = '\0';

    // make tokens of th cmd serv_buf to extract cmd and args
    char *tkn_for_srv = strtok(dup_buf, " ");
    if (tkn_for_srv) {
        strncpy(cmd, tkn_for_srv, sizeof(cmd));
        cmd[sizeof(cmd) - 1] = '\0';
        
        while ((tkn_for_srv = strtok(NULL, " ")) != NULL && usr_agrs_cnt < ARGS_LIMIT - 1) {
            usr_given_agrs[usr_agrs_cnt++] = tkn_for_srv;
        }
        usr_given_agrs[usr_agrs_cnt] = NULL; 
    } else {
        transmitTextReply(sktfd_cli, "Entered command is not valid\n");
        return;
    }

    // compare obtained tokens with usr_sent_cmd names and then process it
    if (strcmp(cmd, "filesrch") == 0) {
        op_selected_filesrch(sktfd_cli, usr_given_agrs);
    } else if (strcmp(cmd, "tarfgetz") == 0) {
        int oper_res = op_selected_tarfgetz(sktfd_cli, usr_given_agrs);
        if (oper_res == 1) {
            transmitTextReply(sktfd_cli, "Err:Something went wrong.");
            printf("Err:Something went wrong in process: tarfgetz\n");
        }
    } else if (strcmp(cmd, "getdirf") == 0) {
        int oper_res = op_selected_getdirf(sktfd_cli, usr_given_agrs);
        if (oper_res == 1) {
            transmitTextReply(sktfd_cli, "Err:Something went wrong.");
            printf("Err:Something went wrong in process: getdirf\n");
        }
    } else if (strcmp(cmd, "fgets") == 0) {
        int oper_res = op_selected_fgetss(sktfd_cli, usr_given_agrs, usr_agrs_cnt);
        if (oper_res == 1) {
            transmitTextReply(sktfd_cli, "Err:Something went wrong.");
            printf("Err:Something went wrong in process: fgets\n");
        }
    } else if (strcmp(cmd, "targzf") == 0) {
        int oper_res = op_selected_targzf(sktfd_cli, usr_given_agrs, usr_agrs_cnt);
        if (oper_res == 1) {
            transmitTextReply(sktfd_cli, "Err:Something went wrong.");
            printf("Err:Something went wrong in process: targzf\n");
        }
    } else if (strcmp(cmd, "quit") == 0) {
        if(usr_agrs_cnt > 0){
        printf("You can not enter arguments with quit!");
        transmitTextReply(sktfd_cli, "Please enter command  in valid format");
        }
        else{
        printf("Quit usr_sent_cmd encountered\n");
        transmitTextReply(sktfd_cli, "Connection will be terminated");
        close(sktfd_cli);
        printf("Connection with client is now terminated.\n");
        exit(0);}
    } else {
        transmitTextReply(sktfd_cli, "Entered usr_sent_cmd is not valid\n");
    }
}

// make a fucntion to proccess the clients
void processClient(int sktfd_cli) {
    // Buffer to store incoming commands
    char buf_for_cmd[BUFF_LEN_LIMIT];
    int content_read;
    transmitTextReply(sktfd_cli, "Enter commands");
    // Continue processing commands until the loop is met with 'quit'
    while (1) {
        memset(buf_for_cmd, 0, sizeof(buf_for_cmd));
        content_read = read(sktfd_cli, buf_for_cmd, BUFF_LEN_LIMIT);
        eradicatebrkLine(buf_for_cmd);

        if (content_read <= 0) {
            printf("Connection terminated.\n");
            break;
        }

        printf("client has transmitted_con: %s\n", buf_for_cmd);
        // send the encountered usr_sent_cmd to process it
        processCommand(sktfd_cli, buf_for_cmd);
    }
}

// make a function to determine where the client will go to server or mirror
char sendClientToServOrMirror(int occurance_of_client) {
    if (occurance_of_client >= 1 && occurance_of_client <= 6) {
        return SRV_CONN;
    } else if (occurance_of_client >= 7 && occurance_of_client <= 12) {
        return MRR_CONN;
    } else {
        static char oper_res = MRR_CONN;  
        oper_res = (oper_res == SRV_CONN) ? MRR_CONN : SRV_CONN;  
        return oper_res;
    }
}

// make a function to transfer client to mirror
int sendClientToMirror(int sktfd_cli){
    AddressInfo address_info;
    long res_mode = STRUCT_RESP;
    send(sktfd_cli, &res_mode, sizeof(res_mode), 0);
    // Send address of mirror and transfer client to that location
    strcpy(address_info.ip_address, MRR_IP);
    address_info.port_number = MRR_PRT;
    send(sktfd_cli, &address_info, sizeof(AddressInfo), 0);
    printf("Client number: %d has been redirected\n", cli_id);

}

// make a function to send text responses to the user
int transmitTextReply(int skt_cli, char* responseText){
    long res_mode = TEXT_RESP;
    send(skt_cli, &res_mode, sizeof(res_mode), 0);
    // transmit reply to client
    send(skt_cli, responseText, strlen(responseText), 0);
    return 0;
}

// make a function to send file responses to user
int transmitFileReply(int diff_skt, const char* name_fl) {
    // Create and open file which will be transmitted
    int fileDesc = open(name_fl, O_RDONLY);
    if (fileDesc < 0) {
        perror(" file operation failed");
        exit(EXIT_FAILURE);
    }
    char serv_buf[RESP_LEN_LIMIT];
    ssize_t obtainedval;
    // send file to cient and read it
    while ((obtainedval = read(fileDesc, serv_buf, 1024)) > 0) {
        send(diff_skt, serv_buf, obtainedval, 0);
    }
    // Close file and socket
    close(fileDesc);
    return 0;
}
    
// helper function - to find information regarding the file
bool findFileInfo(const char* path_fl, char* obtainedreply) {
    struct stat sb;
    if (stat(path_fl, &sb) == 0) {
        time_t fileorigintime;
        fileorigintime = sb.st_mtime;
        char* str_for_time = ctime(&fileorigintime);
        eradicatebrkLine(str_for_time);
        sprintf(obtainedreply, "%s (%lld bytes, created %s)", path_fl, (long long) sb.st_size, str_for_time);
        return true;
    } else {
        sprintf(obtainedreply, "Can not get information for file: %s", path_fl);
        return false;
    }
}

// helperfunction - to seach foor the file entered by the user
bool lookForFile(const char* name_fl, char* path_to_look_at) {
    char* tilde_dir = SRV_HM_DIR;
    char usr_sent_cmd[RESP_LEN_LIMIT];
    sprintf(usr_sent_cmd, "find %s -name '%s' -print -quit", tilde_dir, name_fl);
    FILE* file_ptr_pipe = popen(usr_sent_cmd, "r");
    if (file_ptr_pipe != NULL) {
        if (fgets(path_to_look_at, RESP_LEN_LIMIT, file_ptr_pipe) != NULL) {
            eradicatebrkLine(path_to_look_at);
            pclose(file_ptr_pipe);
            return true;
        }
        pclose(file_ptr_pipe);
    }
    return false;
}

// make a function to  work on the 'filesrch' option 
void op_selected_filesrch(int sktfd_cli, char** usr_given_agrs) {
    char* name_fl = usr_given_agrs[0];
    char obtainedreply[RESP_LEN_LIMIT];
    char path_fl[RESP_LEN_LIMIT];
    if (lookForFile(name_fl, path_fl)) {
        if (findFileInfo(path_fl, obtainedreply)) {
            // Send text obtainedreply to client
            transmitTextReply(sktfd_cli, obtainedreply);
        } else {
            sprintf(obtainedreply, "Err: Can not get file info");
            // Send text obtainedreply to client
            transmitTextReply(sktfd_cli, obtainedreply);
        }
    } else {
        sprintf(obtainedreply, "File not present.");
        // Send text obtainedreply to client
        transmitTextReply(sktfd_cli, obtainedreply);
    }
}

// make a fucntion to recursively look for extension type provided by user
int lookForExt(char *dir_nm, char **type_of_fl, int cnt_for_exten, int *obtainedFlCnt) {
    // status of process 
    int prc_st; 
    // current directory entry point 
    struct dirent *currdirecent;
    // file pointer  
    FILE *ptrForFile; 
    // buffer for path 
    char serv_buf[BUFF_LEN_LIMIT]; 
    // loop counter 
    int loop_incremental; 
    // directory pointer
    DIR *currdir; 
    //opening the current directory 
    if ((currdir = opendir(dir_nm)) == NULL) {
        perror("opendir not working");
        return 1;
    }
    // opeing temporary file to append 
    ptrForFile = fopen(FLLIST_TEMPORARY, "a");
    if (ptrForFile == NULL) {
        fprintf(stderr, "Can not open file\n");
        return 1;
    }
    // looping thru each entry point in directory  
    while ((currdirecent = readdir(currdir)) != NULL) {
        //check if entry is subdirectory 
        if (currdirecent->d_type == DT_DIR && strcmp(currdirecent->d_name, ".") != 0 && strcmp(currdirecent->d_name, "..") != 0) {
            //creating path for sub directory 
            if (dir_nm[strlen(dir_nm) - 1] == '/') {
                sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
            } else {
                sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
            }
            //calling the fucnction recursively for sub directory 
            lookForExt(serv_buf, type_of_fl, cnt_for_exten, obtainedFlCnt);
        } else {
            //ommit . and .. entries 
            if(strcmp(currdirecent->d_name, ".") == 0 || strcmp(currdirecent->d_name, "..") == 0)
                continue;
            // look for user given extension and match file name 
            for (loop_incremental = 0; loop_incremental < cnt_for_exten; loop_incremental++) {
                if (fnmatch(type_of_fl[loop_incremental], currdirecent->d_name, FNM_PATHNAME) == 0) {
                    //make path for matching file 
                    if (dir_nm[strlen(dir_nm) - 1] == '/') {
                        sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
                    } else {
                        sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
                    }
                    // write path into temp file 
                    fprintf(ptrForFile, "%s\n", serv_buf);
                    //increment and get file count  
                    *obtainedFlCnt+=1;
                    break;
                }
            }
        }
    }
    //close current directory and temp file 
    closedir(currdir);
    fclose(ptrForFile);
    return 0;
}

//make a fucntion to handle the usr_sent_cmd 'targzf'
int op_selected_targzf(int sktfd_cli, char **extensions, int cnt_for_exten) {
    //current directory 
    struct dirent *currdirecent;
    //directory pointer 
    DIR *currdir;
    //server home directory 
    const char *tilde_dir = SRV_HM_DIR;
    //array to store files 
    char *type_of_fl[cnt_for_exten];
    //loop counter and obtain files 
    int loop_incremental,obtainedFlCnt = 0;
    //open server home directory  
    if ((currdir = opendir(tilde_dir)) == NULL) {
        perror("opendir not working");
        return 1;
    }
    //file type by appending '*.' to each extension 
    for (loop_incremental = 0; loop_incremental < cnt_for_exten; loop_incremental++) {
        type_of_fl[loop_incremental] = malloc(strlen(extensions[loop_incremental]) + 3);
        snprintf(type_of_fl[loop_incremental], strlen(extensions[loop_incremental]) + 3, "*.%s", extensions[loop_incremental]);
    }
    //look for iles with speciifc extension 
    if (lookForExt(tilde_dir, type_of_fl, cnt_for_exten, &obtainedFlCnt) != 0) {
        perror("Error in finding files");
        closedir(currdir);
        return 1;
    }
    printf("Files found: %d\n", obtainedFlCnt);
    // if not file then send texte to client 
    if (obtainedFlCnt == 0) {
        transmitTextReply(sktfd_cli, "No files can be found.");
        closedir(currdir);
        return 0;
    }
    closedir(currdir);
    //execute tar command to compress file 
    if (execTarCmd(TAR_GZ_TEMPORARY, FLLIST_TEMPORARY) != 0) {
        perror("Error in creating temporary tar file\n");
        return 1;
    }
    // send tar file to client 
    if (transmitFile(sktfd_cli, TAR_GZ_TEMPORARY) != 0) {
        perror("Error in transferring file\n");
        return 1;
    } else {
        printf("File has bee transferred successfully\n");
    }
    //clean temp file 
    cleanupTempFiles();
    return 0;
}

// make a helper function to obtain file's size
long int obtainFileSize(const char *path_fl) {
    FILE *ptrForFile = fopen(path_fl, "rb");
    if (ptrForFile == NULL) {
        return -1; // Error opening file
    }
    fseek(ptrForFile, 0, SEEK_END);
    long int len_of_file = ftell(ptrForFile);
    fclose(ptrForFile);
    return len_of_file;
}

// helper function to remove the temporary tar and text files
void cleanupTempFiles() {
    int prc_st;
    prc_st = remove(FLLIST_TEMPORARY);
    if (prc_st != 0) {
        perror("Error in deleting temp file\n");
    }
    prc_st = remove(TAR_GZ_TEMPORARY);
    if (prc_st != 0) {
        perror("Error in deleting temporary tar file\n");
    }
}

//make a function to transmit the file from server to client
int transmitFile(int sktfd_cli, const char *path_fl) {
    FILE *ptrForFile = fopen(path_fl, "rb");
    if (ptrForFile == NULL) {
        return 1; // Error opening file
    }
    long int len_of_file = obtainFileSize(path_fl);
    if (len_of_file == -1) {
        fclose(ptrForFile);
        return 1; // Error getting file size
    }
    if (send(sktfd_cli, &len_of_file, sizeof(len_of_file), 0) != sizeof(len_of_file)) {
        fclose(ptrForFile);
        return 1; // Error sending file size
    }
    if (transmitFileReply(sktfd_cli, path_fl) != 0) {
        fclose(ptrForFile);
        return 1; // Error transferring file
    }
    fclose(ptrForFile);
    return 0;
}

// make a function to recursively look for the file names which user inputs into the usr_sent_cmd
int lookForName(char *dir_nm, char **nms_of_fl, int file_count, int *obtainedFlCnt) {
    //file pointer for temp file 
    FILE *ptrForFile;
    //current dir entry point 
    struct dirent *currdirecent;
    //loop counter 
    int loop_incremental;
    //buffer for contr path 
    char serv_buf[BUFF_LEN_LIMIT];
    //directory pointer 
    DIR *currdir;
    //pointer 
    int prc_st;
    //open specific directory 
    if ((currdir = opendir(dir_nm)) == NULL) {
        printf("opendir not working");
        return 1;
    }
    //opening temop file for appending 
    ptrForFile = fopen(FLLIST_TEMPORARY, "a");
    if (ptrForFile == NULL) {
        fprintf(stderr, "Can not open file\n");
        return 1;
    }
    //loop thru each entry in directory 
    while ((currdirecent = readdir(currdir)) != NULL) {
        //check if curr directory 
        if (currdirecent->d_type == DT_DIR && strcmp(currdirecent->d_name, ".") != 0 && strcmp(currdirecent->d_name, "..") != 0) {
            // path for sundirectory 
            if (dir_nm[strlen(dir_nm) - 1] == '/') {
                sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
            } else {
                sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
            }
            // calling func recursively for sub directory 
            lookForExt(serv_buf, nms_of_fl, file_count, obtainedFlCnt);
        } else {
            // ommit . and .. entriers 
            if(strcmp(currdirecent->d_name, ".") == 0 || strcmp(currdirecent->d_name, "..") == 0)
                continue;
            for (loop_incremental = 0; loop_incremental < file_count; loop_incremental++) {
                if (strcmp(nms_of_fl[loop_incremental], currdirecent->d_name) == 0) {
                    //full path for matching file 
                    if (dir_nm[strlen(dir_nm) - 1] == '/') {
                        sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
                    } else {
                        sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
                    }
                    //write the match file path to temporary file 
                    fprintf(ptrForFile, "%s\n", serv_buf);
                    //increment obtained file count 
                    *obtainedFlCnt+=1;
                    break;
                }
            }
        }
    }
    //closed the directory and temporary file 
    closedir(currdir);
    fclose(ptrForFile);
    return 0;
}

//Make a function to handle the usr_sent_cmd 'fgets'
int op_selected_fgetss(int clisock_fd, char **name_fl, int cnt_f_nm)
{   
    DIR *currdir;
    char *cmd_for_fgets = malloc(sizeof(char) * BUFF_LEN_LIMIT);
    char *cmd_format_of_fgets = "tar -czf %s -T %s";
    char *tilde_dir = SRV_HM_DIR;
    int loop_incremental, prc_st;
    char nm_fl[BUFF_LEN_LIMIT-21];
    
    memset(nm_fl, 0, sizeof(nm_fl));
    
    if ((currdir = opendir(tilde_dir)) == NULL) {
        perror("opendir not working");
        return 1;
    }

    int obtainedFlCnt = 0;
    if (lookForName(tilde_dir, name_fl, cnt_f_nm, &obtainedFlCnt) != 0) {
        perror("Error in finding nm_fl");
        return 1;
    }

    if (obtainedFlCnt == 0)
    {
        // None of the nm_fl exist
        transmitTextReply(clisock_fd, "No file found");
        return 0;
    }
    sprintf(cmd_for_fgets, cmd_format_of_fgets, TAR_GZ_TEMPORARY, FLLIST_TEMPORARY);;
    system(cmd_for_fgets);
    FILE *ptr_fl;
    char *serv_buf;
    long fl_sz;
    long int fl_sz_for_op;

    ptr_fl = fopen(TAR_GZ_TEMPORARY, "rb");

    if (ptr_fl == NULL)
    {
        perror("Error in opening file\n");
        return 1;
    }
    fseek(ptr_fl, 0, SEEK_END);
    fl_sz_for_op = ftell(ptr_fl);

    fseek(ptr_fl, 0, SEEK_SET);
    serv_buf = (char *)malloc((fl_sz + 1) * sizeof(char)); 
    fread(serv_buf, fl_sz, 1, ptr_fl);                  
    send(clisock_fd, &fl_sz_for_op, sizeof(fl_sz_for_op), 0);

    // Transfer the file
    int res_fl_temp = transmitFileReply(clisock_fd, TAR_GZ_TEMPORARY);
    if (res_fl_temp != 0)
    {
        perror("Error in transferring file\n");
        return 1;
    }
    else
    {
        printf("File has been transferred successfully\n");
    }
    prc_st = remove(FLLIST_TEMPORARY);
    if (prc_st != 0) {
        perror("Error in deleting temporary file list\n");
        return 0;
    }
    prc_st = remove(TAR_GZ_TEMPORARY);
    if (prc_st != 0) {
        perror("Error in deleting temporary tar\n");
        return 0;
    }
    fclose(ptr_fl);
    return 0;
}

//make a function to recursively look for the size which user puts in as input
int lookForSize(char *dir_nm, int usr_giv_sz1, int usr_giv_sz2, int *obtainedFlCnt)
{   //file pointer for temp file 
    FILE *ptrForFile;
    //buffer for path making 
    char serv_buf[BUFF_LEN_LIMIT];
    //current directory entry 
    struct dirent *currdirecent;
    //directory pointer 
    DIR *currdir;
    memset(serv_buf, 0, sizeof(serv_buf));
    // open root directory
    if ((currdir = opendir(dir_nm)) == NULL) {
        perror("Error in opening currdir.\n");
        return 1;
    }
    // open temporary file list
    ptrForFile = fopen(FLLIST_TEMPORARY, "a");
    if (ptrForFile == NULL) {
        perror("Can not open file\n");
        return 1;
    }
    //loop thru each entry in directory 
     while ((currdirecent = readdir(currdir)) != NULL)
    {   // skip . & .. entrires 
        if (strcmp(currdirecent->d_name, ".") == 0 || strcmp(currdirecent->d_name, "..") == 0)
            continue;     
        // make full path for entries       
        if (dir_nm[strlen(dir_nm) - 1] == '/') 
        {
                sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
        } 
        else 
        {
            sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
        }
        //structure to contain file info
        struct stat buf_for_src;
        //get info using stat
        if (stat(serv_buf, &buf_for_src) == -1)
        {
            continue;
        }
        //check if entry is directory 
        if (currdirecent->d_type == DT_DIR)
        {   // recusicely calls func for subdirectory 
            lookForSize(serv_buf, usr_giv_sz1, usr_giv_sz2, obtainedFlCnt);
        }
        else 
       {
            // to chcek if entry is regular file within size range 
           if (S_ISREG(buf_for_src.st_mode) && buf_for_src.st_size >= usr_giv_sz1 && buf_for_src.st_size <= usr_giv_sz2 && usr_giv_sz1 <= usr_giv_sz2) 
           {
                // write file path in temp file
                if (dir_nm[strlen(dir_nm) - 1] == '/') {
                    sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
                } else {
                    sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
                }
                fprintf(ptrForFile, "%s\n", serv_buf);
                //increase obtained file count 
                *obtainedFlCnt+=1;
           }
        }
    }
    //close directory and temp file 
    closedir(currdir);
    fclose(ptrForFile);
    return 0;
}

//make a function to handle the usr_sent_cmd 'tarfgetz'
int op_selected_tarfgetz(int sktfd_cli, char **usr_given_agrs) {
    int obtainedFlCnt = 0;
    size_t usr_giv_sz1 = atoi(usr_given_agrs[0]);
    char *tilde_path = SRV_HM_DIR;
    size_t usr_giv_sz2 = atoi(usr_given_agrs[1]);
    lookForSize(tilde_path, usr_giv_sz1, usr_giv_sz2, &obtainedFlCnt);

    if (obtainedFlCnt > 0) {
        execTarCmd(TAR_GZ_TEMPORARY, FLLIST_TEMPORARY);
        transmitTarFile(sktfd_cli, TAR_GZ_TEMPORARY);
    } else {
        transmitTextReply(sktfd_cli, "No files found.");
    }

    cleanupTempFiles();
    return 0;
}

// helper function to execute the tar usr_sent_cmd
int execTarCmd(const char *final_file, const char *temp_file_list) {
    char serv_buf[BUFF_LEN_LIMIT];
    char *cmd_format_of_fgets = "tar -czf %s -T %s";
    
    snprintf(serv_buf, sizeof(serv_buf), cmd_format_of_fgets, final_file, temp_file_list);
    int prc_st = system(serv_buf);
    
    if (prc_st == 0) {
        printf("Command executed successfully\n");
    } else {
        perror("Error executing command\n");
    }
    return prc_st;
}

// make a function to transmit the tar file to user
int transmitTarFile(int sktfd_cli, const char *path_fl) {
    FILE *ptr_fl;
    char *serv_buf;
    long len_of_file;

    ptr_fl = fopen(path_fl, "rb"); // Open the file in binary mode

    if (ptr_fl == NULL) {
        perror("Error opening file\n");
        return 1;
    }

    fseek(ptr_fl, 0, SEEK_END);
    len_of_file = ftell(ptr_fl);
    fseek(ptr_fl, 0, SEEK_SET);
    serv_buf = (char *)malloc(len_of_file * sizeof(char)); 
    fread(serv_buf, len_of_file, 1, ptr_fl);              
    send(sktfd_cli, &len_of_file, sizeof(len_of_file), 0);
    size_t transmitted_bt = 0;
    size_t leftover_con = len_of_file;
    while (leftover_con > 0) {
        size_t chunk_size = (leftover_con > BUFF_LEN_LIMIT) ? BUFF_LEN_LIMIT : leftover_con;
        ssize_t transmitted_con = send(sktfd_cli, serv_buf + transmitted_bt, chunk_size, 0);
        if (transmitted_con == -1) {
            perror("Error sending file data\n");
            free(serv_buf);
            fclose(ptr_fl);
            return 1;
        }
        transmitted_bt += transmitted_con;
        leftover_con -= transmitted_con;
    }
    printf("File transmitted_con successfully\n");
    fclose(ptr_fl); // Close the file
    free(serv_buf);
    return 0;
}

//make a function to recursively look for the dates that the user inputs
int lookForDate(char *dir_nm, time_t dn1, time_t dn2, int *obtainedFlCnt)
{   
    //buffer for builidng path
    char serv_buf[BUFF_LEN_LIMIT];
    //pointer for diectory 
    DIR *currdir;
    //current directory entry 
    struct dirent *currdirecent;
    //temp file pointer 
    FILE *ptrForFile;
    
    memset(serv_buf, 0, sizeof(serv_buf));
    //open specific directory 
    if ((currdir = opendir(dir_nm)) == NULL) {
        perror("Error opening directory.\n");
        return 1;
    }  
    // Open temp file list 
    ptrForFile = fopen(FLLIST_TEMPORARY, "a");
    if (ptrForFile == NULL) {
        perror("Failed to open file\n");
        return 1;
    }
    //loop thru entry in directory 
    while ((currdirecent = readdir(currdir)) != NULL)
    {
        // Skip . and ..
        if (strcmp(currdirecent->d_name, ".") == 0 || strcmp(currdirecent->d_name, "..") == 0)
        {
            continue;
        }
        //build full path
        if (dir_nm[strlen(dir_nm) - 1] == '/') {
                sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
        } else {
            sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
        }
        // check for directory 
        if (currdirecent->d_type == DT_DIR)
        {   //recursive func call for subdirectory 
            lookForDate(serv_buf, dn1, dn2, obtainedFlCnt);
        }
        else{
            //structure to hold file info 
            struct stat buf_for_src;
            //get file info using stat
            if (stat(serv_buf, &buf_for_src) == -1)
            {   //skip if not able to get file info 
                continue;
            }
            time_t fileorigintime;
            //modified time of file 
            fileorigintime = buf_for_src.st_mtime;
            //to check of file modification time is within specified date range 
            if (fileorigintime >= dn1 && fileorigintime <= dn2)
            {   
                if (dir_nm[strlen(dir_nm) - 1] == '/') {
                    sprintf(serv_buf, "%s%s", dir_nm, currdirecent->d_name);
                } else {
                    sprintf(serv_buf, "%s/%s", dir_nm, currdirecent->d_name);
                }
                // Write matching file's path to temp file
                fprintf(ptrForFile, "%s\n", serv_buf);
                *obtainedFlCnt+=1;
            }
        }
    }
    //close directory and temp file 
    closedir(currdir);
    fclose(ptrForFile);
    return 0;
}

//make a fucntion  to handlt the usr_sent_cmd 'getdirf'
int op_selected_getdirf(int sktfd_cli, char **usr_given_agrs) {
    int op_getdirf_st;
    time_t dn1 = transformDateIntoEnvType(usr_given_agrs[0], 1);
    int obtainedFlCnt = 0;
    time_t dn2 = transformDateIntoEnvType(usr_given_agrs[1], 2);
    lookForDate(SRV_HM_DIR, dn1, dn2, &obtainedFlCnt);
    op_getdirf_st=execTarCmd(TAR_GZ_TEMPORARY, FLLIST_TEMPORARY);
    if (obtainedFlCnt > 0) {
        if (op_getdirf_st == 0) {
            printf("Command executed successfully\n");
            transmitTarFile(sktfd_cli, TAR_GZ_TEMPORARY);
        } else {
            perror("Error executing command\n");
            return 1;
        }
    } else {
        perror("No files found.\n");
        transmitTextReply(sktfd_cli, "No files found.");
    }
    cleanupTempFiles();
    return 0;
}
// make a function to eradicate linebreaks withhin the usr_sent_cmd
void eradicatebrkLine(char serv_buf[]) {
    int len_str = strlen(serv_buf);
    if (len_str > 0 && serv_buf[len_str - 1] == '\n') {
        serv_buf[len_str - 1] = '\0';
    }
}

// helper function - to parse the date string
int parseDateString(const char *str_for_tm, struct tm *strct_for_tm) {
    int in_y;
    int in_m;
    int in_d;
    if (sscanf(str_for_tm, "%d-%d-%d", &in_y, &in_m, &in_d) != 3) {
        return 0; // Parsing error
    }
    strct_for_tm->tm_year = in_y - 1900;
    strct_for_tm->tm_mon = in_m - 1;
    strct_for_tm->tm_mday = in_d;
    strct_for_tm->tm_isdst = -1;
    return 1; // Successful parsing
}

// helper function to adjusttime
void adjustTime(struct tm *strct_for_tm, int valid_day_format) {
    if (valid_day_format == 1) {
        strct_for_tm->tm_hour = 0;
        strct_for_tm->tm_min = 0;
        strct_for_tm->tm_sec = 0;
    } else {
        strct_for_tm->tm_hour = 23;
        strct_for_tm->tm_min = 59;
        strct_for_tm->tm_sec = 59;
    }
}

// make a function to transform date form from the user mentioned to sys type date format
time_t transformDateIntoEnvType(const char *str_for_tm, int valid_day_format) {
    struct tm tm;

    if (!parseDateString(str_for_tm, &tm)) {
        return (time_t)-1; // Parsing error
    }

    adjustTime(&tm, valid_day_format);

    time_t t = mktime(&tm);
    if (t == (time_t)-1) {
        return (time_t)-1; // Conversion error
    }

    return t; // Successful conversion
}
