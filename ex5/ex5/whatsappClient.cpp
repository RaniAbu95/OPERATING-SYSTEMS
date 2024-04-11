#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <thread>
#include <stdio.h>
#include <libltdl/lt_system.h>

using namespace std;
#define ERR -1
#define EQ 0

#define BUFF_SIZE 256

int client;
int portNum ;
char buffer[BUFF_SIZE];
char* ip ;
string clientName;
string groupName;

/**
 * check if the command legal
 * @param cmd - the comand line
 * @return 1 if is ligal , -1 otherwise
 */
int is_command_legal(string cmd)
{
    char* buf = strdup(cmd.c_str());
    char *cmd_arg = strtok(buf, " ");
    if (strcmp(cmd_arg, "create_group") == EQ)
    {
        cmd_arg = strtok(NULL," ");
        if(cmd_arg == NULL)
        {
            cout<< "ERROR: Invalid input.\n"<<endl;
            return ERR;
        }
        else
        {
            groupName = cmd_arg;
        }

    }
    else if (strcmp(cmd_arg, "send") == EQ)
    {
        cmd_arg = strtok(NULL," ");
        if(cmd_arg == NULL)
        {
            cout<< "ERROR: Invalid input.\n"<<endl;
            return ERR;
        }
        cmd_arg = strtok(NULL,"");
        if(cmd_arg == NULL)
        {
            cout<< "ERROR: Invalid input.\n"<<endl;
            return ERR;
        }
    }
    else if (strcmp(cmd_arg, "who") == EQ)
    {
        cmd_arg = strtok(NULL," ");
        if(cmd_arg != NULL)
        {
            cout<< "ERROR: Invalid input.\n"<<endl;
            return ERR;
        }
    }
    else if(strcmp(cmd_arg, "exit") == EQ)
    {
        cmd_arg = strtok(NULL," ");
        if(cmd_arg != NULL)
        {
            cout<< "ERROR: Invalid input.\n"<<endl;
            return ERR;
        }
    } else{
        cout<< "ERROR: Invalid input.\n"<<endl;
        return ERR;
    }
    return 1;

}

/**
 * the read function that respon to get msg
 */
void thread_read(){
    char buf [256];
    string quote =  "\"";
    string group_err = "ERROR: failed to create group " + quote + groupName + quote;
    while (true) {
        memset(buf,'\0',256);
        ssize_t val = recv(client, buf, 256, 0);
        if(strcmp(buf,"SERVER EXIT.\n") == EQ)
        {
            exit(0);
        }
        cout << buf <<endl;
        if(strcmp(buf,"Unregistered successfully.\n") == EQ)
        {
            break;
        }
        else if (strcmp(buf,group_err.c_str()) == EQ) {
            cout << group_err << endl;
        }

    }
}

/**
 * the write function that respon to write and send a massges
 */
void thread_write(){
    while(true){
        string command;
        getline(std::cin, command);
        if(is_command_legal(command) == 1)
        {
            send(client, command.c_str(), BUFF_SIZE, 0);
        }
        if (strcmp(command.c_str(),"exit") == EQ)
        {
            send(client, command.c_str(), BUFF_SIZE, 0);
            break;
        }

    }

}



int main(int argc , char *argv[])
{

    if(argc != 4)
    {
        cout<<"Usage: whatsappClient clientName serverAddress serverPort"<<endl;
    }

    clientName = argv[1];
    ip =  argv[2];
    portNum = atoi(argv[3]);

    struct sockaddr_in server_addr;

//    client = socket(AF_INET, SOCK_STREAM, 0);
    if( (client = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        cerr << "ERROR: socket " << client <<"."<<endl;
        exit(EXIT_FAILURE);
    }


    if (client < 0)
    {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }




    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portNum);

    // this function returns returns 1 if the IP is valid
    // and 0 if invalid
    // inet_pton converts IP to packets
    // inet_ntoa converts back packets to IP
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(client,(struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
    {

        string str = clientName;
        str = "join "+ str;
        send(client, str.c_str(), BUFF_SIZE, 0);
    }



    recv(client, buffer, BUFF_SIZE, 0);//
    if (strcmp(buffer, "client already connected") == EQ)
    {
        cout<<"Client name is already in use."<<endl;
        close(client);
        exit(1);
    }

    cout << buffer << endl;

    int valread;
    bool isExit = false;
    thread thread1 = thread(thread_read);
    thread thread2 = thread(thread_write);
    thread1.join();
    thread2.join();

    close(client);
    return 0;
}
