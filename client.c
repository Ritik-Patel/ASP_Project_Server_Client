/*
Submitted by: Ritik Patel, Gunjan Kumar Kalathia
ID: 110100962, 110094776
Section 1
*/

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define S_PRT 8000
#define MAX_ARGS 10
#define SIZE_OF_BUFF 1024
#define TEXT_RESP 1
#define STRUC_RESP 2
#define FILE_RESP 3

// Creating a structure to transfer IP of mirror sort along with its port number
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    int port_number;
} AddressInfo;

// Making flag like variables for checking whether file is obtained, is to be exited or is to be unzipped
int toBeUnzipped = 0, toBeExited = 0, isFileObtained = 0;

//---------- Define functions for pre proccessing the inputs ----------//

// Declaring a function to check and validate the commands input by user 
int chkVldInp(char* org_buff);
// Declaring a function to check for <-u> unzip option
void chk_for_u_option(char org_buff[]);
// Declaring a function to remove linebreaks
void eradicateBrkLine(char org_buff[]);
// Declaring a function to check and validate input dates
int chkForValidDate(char date[]);
// Declaring a function to check and validate input digits for size
int chkForAllNumDigits(char* str);

//---------- Define functions for handling connections ----------//

// Declaring a function to connect client with server
int servConnFunc(const char *ip_for_serv);
// Declaring a function to connect client with mirror
int MrrConnFunc(const char *ip_for_mrr, int port_for_mrr);
// Declaring a function to get response from client and process it to send to server/mirror
void getAndWorkClientResp(int cli_sckt, char *flname);
// Declaring a function to unzip the tar files when needed
void doUnzip(const char *flname);

//---------- Defining Logic of each function ----------//

// Main function
int main(int argc, char *argv[]) {
    char ip_for_serv[16];
    // Declaring a file to store the output
    char *flname = "temp.tar.gz";
    // If user doesnot enter server's ip then prompt user to enter it
    if (argc < 2) {
        printf("Synopsis: %s <ip_for_serv>\n", argv[0]);
        return 1;
    }
    strcpy(ip_for_serv, argv[1]);
    // connect the client to server and then parse the user'sinput
    int cli_sckt = servConnFunc(ip_for_serv);
    getAndWorkClientResp(cli_sckt, flname);

    // Close connection to Server / Mirror
    close(cli_sckt);
    return 0;
}

// We make a method to connect user's client with the server using socket
int servConnFunc(const char *ip_for_serv) {
    int cli_sckt = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_sckt < 0) {
        printf("Err in socket generation\n");
        exit(1);
    }
    // following the general rules for creating the socket connection
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(S_PRT);
    // prompt user that the address of server is not valid
    if (inet_pton(AF_INET, ip_for_serv, &serverAddress.sin_addr) <= 0) {
        printf("Invalid Address or Address not supported\n");
        exit(1);
    }
    // prompt user if the connection is not established 
    if (connect(cli_sckt, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Failed in establishing connection\n");
        exit(1);
    }

    return cli_sckt;
}

// We make a method to take in user's input commands and then parse it and work it into the server
void getAndWorkClientResp(int cli_sckt, char *flname) {
    long firstResp = 0;
    // function to receive respons from socket connection
    recv(cli_sckt, &firstResp, sizeof(firstResp), 0);

    
    // give the greeting message
    if (firstResp == TEXT_RESP) {
        printf("Connected successfully to the server!\n");
        char servReply[SIZE_OF_BUFF];
        memset(servReply, 0, sizeof(servReply));
        read(cli_sckt, servReply, sizeof(servReply));
        printf("Server's response: %s\n", servReply);
    } 
    // print this if redirected to the mirror server
    else if (firstResp == STRUC_RESP) {
        printf("Will be redirected to mirror.\n");
        AddressInfo transferToMirror;
        recv(cli_sckt, &transferToMirror, sizeof(AddressInfo), 0);
        close(cli_sckt);
        cli_sckt = MrrConnFunc(transferToMirror.ip_address, transferToMirror.port_number);
        printf("Connected to mirror server now!\n");
        char servReply[SIZE_OF_BUFF];
        memset(servReply, 0, sizeof(servReply));
        read(cli_sckt, servReply, sizeof(servReply));
        printf("Server's Response: %s\n", servReply);
    }

    while (1) {
        char buffForRetrivedCmd[SIZE_OF_BUFF] = {0};
        isFileObtained = 0;
        // take commands from user and store them in buffForRetrivedCmd
        printf("\nC$: ");
        fgets(buffForRetrivedCmd, SIZE_OF_BUFF, stdin);
        // eradicate the linebreaks and check for <-u>
        eradicateBrkLine(buffForRetrivedCmd);
        chk_for_u_option(buffForRetrivedCmd);

        char obtainedBuffVal[SIZE_OF_BUFF];
        strcpy(obtainedBuffVal, buffForRetrivedCmd);
        // check if what user entered is valid or not
        if (chkVldInp(obtainedBuffVal))
            continue;
        // transmit message to the server/mirror socket
        send(cli_sckt, buffForRetrivedCmd, strlen(buffForRetrivedCmd), 0);
        // read what server/mirror transmitts
        long cli_hdr;
        int readForVal = read(cli_sckt, &cli_hdr, sizeof(cli_hdr));

        // if client can read value from socket then process what the user asks for
        if (readForVal > 0) {
            if (cli_hdr == TEXT_RESP) {
                char servReply[SIZE_OF_BUFF];
                memset(servReply, 0, sizeof(servReply));
                read(cli_sckt, servReply, sizeof(servReply));
                printf("Server responds: %s\n", servReply);
            } else {
                isFileObtained = 1;
                long spaceOfFile = cli_hdr;

                char fileForServReply[SIZE_OF_BUFF];
                FILE *pointerForFile = fopen(flname, "wb");
                if (pointerForFile == NULL) {
                    printf("Err: Can not create the file.\n");
                    exit(1);
                }

                time_t timeBegin;
                time(&timeBegin);

                long bytObtained = 0;
                while (bytObtained < spaceOfFile) {
                    int bytToGet = SIZE_OF_BUFF;
                    if (bytObtained + SIZE_OF_BUFF > spaceOfFile) {
                        bytToGet = spaceOfFile - bytObtained;
                    }
                    int finalBytObtained = recv(cli_sckt, fileForServReply, bytToGet, 0);
                    if (finalBytObtained < 0) {
                        printf("Err: Can not get file data");
                        exit(1);
                    }
                    fwrite(fileForServReply, sizeof(char), finalBytObtained, pointerForFile);
                    bytObtained += finalBytObtained;
                    if (bytObtained >= spaceOfFile) {
                        break;
                    }
                }
                printf("\nObtained file: %s\n", flname);
                fclose(pointerForFile);
                if (toBeUnzipped && isFileObtained) {
                doUnzip(flname);
                toBeUnzipped = 0;
            }
            }
        } else {
            printf("Connection with server will be terminated now\n");
            break;
        }

        if (toBeExited) {
            printf("Connection successfully terminated.\n");
            break;
        }

        if (toBeUnzipped && isFileObtained) {
            chk_for_u_option(flname);
            toBeUnzipped = 0;
        }
    }
}

// We make a function to do the unzip of the tar file
void doUnzip(const char *flname) {
    char cmd[SIZE_OF_BUFF];
    snprintf(cmd, SIZE_OF_BUFF, "tar -xzf %s", flname);
    system(cmd);
    printf("File sent by server has also been unzipped as needed\n");
}

// We make a function to connect the client to mirror server
int MrrConnFunc(const char *ip_for_mrr, int port_for_mrr) {
    int cli_sckt = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_sckt < 0) {
        printf("Cannot create client socket\n");
        exit(1);
    }

    struct sockaddr_in mirrorAddress;
    mirrorAddress.sin_family = AF_INET;
    mirrorAddress.sin_port = htons(port_for_mrr);
    if (inet_pton(AF_INET, ip_for_mrr, &mirrorAddress.sin_addr) <= 0) {
        printf("Err: Wrong Mirror Address\n");
        exit(1);
    }

    if (connect(cli_sckt, (struct sockaddr *)&mirrorAddress, sizeof(mirrorAddress)) < 0) {
        printf("Err: Cannot conect to mirror.\n");
        exit(1);
    }

    return cli_sckt;
}

int chkVldInp(char* buffForRetrivedCmd) {
    char *entered_args[MAX_ARGS];
    int argc_count = 0;

    // Tokenize input
    char* cmdtkn = strtok(buffForRetrivedCmd, " "); 
    while (cmdtkn != NULL) {
        entered_args[argc_count++] = cmdtkn; 
        cmdtkn = strtok(NULL, " "); 
    }
    
    // Set the last element of the array to NULL
    entered_args[argc_count] = NULL; 
    
    // List of valid commands
    const char* allowed_cmds[] = {
        "fgets", "filesrch", "tarfgetz", "targzf", "getdirf", "quit"
    };
    int val_cmd_count = sizeof(allowed_cmds) / sizeof(allowed_cmds[0]);

    // Check if the command entered is valid
    int chkCmdValidity = 0;
    for (int i = 0; i < val_cmd_count; ++i) {
        if (strcmp(entered_args[0], allowed_cmds[i]) == 0) {
            chkCmdValidity = 1;
            break;
        }
    }
    if (!chkCmdValidity) {
        printf("You entered invalid command\n");
        return 1;
    }
    
    // Check that entered_args are given along with the command 
    if (argc_count < 2) {
    if (strcmp(entered_args[0], "quit") != 0) {
        printf("Err: You need to enter arguments along with command!\n");
        return 1;
    }
}
    // Check that entered_args do not exceed maximum possible
    else if (argc_count > 7) {
        printf("Err: Max number of args exceeded!\n");
        return 1;
    }
    
    // Validate entered_args for command : filesrch
    if (strcmp(entered_args[0], "filesrch") == 0 && argc_count > 2) {
        printf("Err: Enter too many argument(s) along with filesrch!\n");
        return 1;
    }

	// Validate entered_args for command : fgets
    if (strcmp(entered_args[0], "fgets") == 0) {
    if (argc_count < 2) {
        printf("Synopsis:fgets file1 file2 file3 file4\n");
        return 1;
    }
    if (argc_count > 5) {
        printf("Too many arguments entered!\n");
        return 1;
    }
    }
    // Validate entered_args for command : targzf
    if (strcmp(entered_args[0], "targzf") == 0) {
    if (argc_count < 2) {
        printf("Usage: targzf <ext1> <ext2> ... <-u>\n");
        return 1;
    }
    if (argc_count > 5) {
        printf("Too many arguments entered!\n");
        return 1;
    }
    if (strcmp(entered_args[argc_count - 1], "-u") == 0) {
        if (argc_count < 3) {
            printf("Synopsis: targzf <ext1> <ext2> ... <-u>\n");
            return 1;
        }
    }
    }
    
    // Validate entered_args for command : tarfgetz
    if (strcmp(entered_args[0], "tarfgetz") == 0) {
        if ( argc_count < 2 || argc_count > 3) {
            printf("Synopsis: tarfgetz size1 size2 <-u>\n");
            return 1;
        }
        if (chkForAllNumDigits(entered_args[1]) == 0 || chkForAllNumDigits(entered_args[2]) == 0) {
            printf("Either Size 1 or Size 2 entered is incorrect\n");
            printf("Synopsis: tarfgetz size1 size2 <-u>\n");
            return 1;
        }
        int size1 = atoi(entered_args[1]);
        int size2 = atoi(entered_args[2]);
        if (size1 > size2) {
            printf("Size 1 must be smaller than Size 2!\n");
            return 1;
        }
    }
    
    // Validate entered_args for command : getdirf
    if (strcmp(entered_args[0], "getdirf") == 0) {
        if ( argc_count < 2 || argc_count > 3) {
            printf("Synopsis: getdirf date1 date2 <-u>\n");
            return 1;
        }
        if (chkForValidDate(entered_args[1]) == 0 || chkForValidDate(entered_args[2]) == 0) {
            printf("Date 1 or Date 2 entered is incorrect\n");
            printf("Synopsis: getdirf date1 date2 <-u>\n");
            return 1;
        }
        // Compare dates using direct comparison
        struct tm tm1 = { 0 };
        struct tm tm2 = { 0 };
        sscanf(entered_args[1], "%d-%d-%d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);
        sscanf(entered_args[2], "%d-%d-%d", &tm2.tm_year, &tm2.tm_mon, &tm2.tm_mday);
        time_t date1 = mktime(&tm1);
        time_t date2 = mktime(&tm2);
        if (date1 > date2) {
            printf("Date 1 must be occuring before Date 2!\n");
            return 1;
        }
    }    
    if (strcmp(entered_args[0], "quit") == 0) {
    	if(argc_count > 1){
    		printf("You can not enter any arguments with quit \n");
    	}
    	else{
        toBeExited = 1;
        }
    }
    
    // Prevent unzip with fgets, filesrch and quit
    if ((strcmp(entered_args[0], "filesrch") == 0 || strcmp(entered_args[0], "quit") == 0 || strcmp(entered_args[0], "fgets") == 0) &&
        toBeUnzipped == 1) {
        toBeUnzipped = 0;
        toBeExited = 0;
        printf("Err: You can not use -u with filesrch,fgets or quit\n");
        return 1;
    }
    
    return 0;
}

// We make a function to check if user has entered <-u> option
void chk_for_u_option(char org_buff[]) {
    int size_of_buffer = strlen(org_buff);
    if (size_of_buffer >= 3 && org_buff[size_of_buffer - 3] == ' ' && org_buff[size_of_buffer - 2] == '-' && org_buff[size_of_buffer - 1] == 'u') {
        toBeUnzipped = 1;
        org_buff[size_of_buffer - 3] = '\0';
    }
}

// We make a function to eradicate all linebreaks
void eradicateBrkLine(char org_buff[]) {
    int i = 0;
    while (org_buff[i] != '\0') {
        if (org_buff[i] == '\n') {
            org_buff[i] = '\0';
            break;  // Remove only the first line break encountered
        }
        i++;
    }
}

// We make a function to check if the year entered by user is leap year or not
int isLeapYear(int in_y) {
    return (in_y % 4 == 0 && in_y % 100 != 0) || (in_y % 400 == 0);
}

// We make a method to check day in months
int isValidDay(int in_y, int in_m, int in_d) {
    switch (in_m) {
    	// For April, June, September and November day must lie between 1 to 30
        case 4: case 6: case 9: case 11:
            return (in_d >= 1 && in_d <= 30);
        // For February date must lie between 1 to 28 in normal year and 1 to 29 in leap year
        case 2:
            return (isLeapYear(in_y) ? (in_d >= 1 && in_d <= 29) : (in_d >= 1 && in_d <= 28));
        // for other months day must lie between 1 to 31
        default:
            return (in_d >= 1 && in_d <= 31);
    }
}

// We make a method to check if the date entered by user in getdirf are valid or not
int chkForValidDate(char date[]) {
    int year, month, day;
    // We taked YYYY-MM-DD format of input from user
    if (sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        // Failure in in entered input
        return 0;
    }
    // We define a range for years from 1000 to 10000
    if (year < 1000 || year > 9999) {
        return 0;
    }
    // We check if month lies between 1-12
    if (month < 1 || month > 12) {
        return 0;
    }
    // We check if date entered is valid or not for that month
    if (!isValidDay(year, month, day)) {
        return 0;
    }
    // When the entered dates passes all condition, the date is valid
    return 1;
}

// We make a function to check if all enetered numbers are digits or not
int chkForAllNumDigits(char* str) {
    while (*str) {
        if (!isdigit(*str)) {
            return 0;  // Not every character enter is a digit
        }
        str++;
    }
    return 1;  // All characters are digits
}