#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <algorithm>
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <iostream>
#include <libltdl/lt_system.h>
#include <thread>

#define TRUE   1
#define EQ 0
#define SEND_FLAG 0
using namespace std;

std::map <string,vector<int>> clientGroups;
std::map <string,int> clientNames;
fd_set readfds;

struct sockaddr_in address;
int max_clients = 30;
char **argv1;
int client_socket[30];
int master_socket;

/**
 * the function get a name of the client
 * @param client_sd - the sd of the client
 * @return the name of the client
 */
string get_clientName(int client_sd)
{
    for(map<string,int>::iterator it = clientNames.begin(); it != clientNames.end(); it ++)
    {
        if(it->second == client_sd)
        {
            return it->first;
        }
    }
    return NULL;
}


/**
 * send a message to the socket with the given sd.
 * @param receiver_sd the sd of the reciver socket
 * @param message the message to send
 * @param sender_sd the sender sd.

 */
void sendmsg(int receiver_sd, char* message,int sender_sd, bool is_group_conv)
{

    string senderName,finalMsg;
    for(map<string,int>::iterator it = clientNames.begin();it != clientNames.end();it++)
    {
        if(it->second == sender_sd)
        {
            senderName=it->first;
        }
    }
    finalMsg= senderName+":"+message;

    send(receiver_sd,finalMsg.c_str(),finalMsg.size(), SEND_FLAG);

    if(!is_group_conv)
    {
        cout << get_clientName(sender_sd)<<": "<<"\""<<message<<"\""<<" was sent successfully to " <<get_clientName(receiver_sd)<<"."<<endl;
    }

}

/**
 * creates a group with the given name.
 * @param groupName the name of the grop
 * @param members_sd the sd of the sockets members.
 */
void create_group(string groupName, vector<int> members_sd)
{

    clientGroups[groupName] = members_sd;
}

/**
 * a compare function between two string by alphabetical
 * @param s1 the first string
 * @param s2 the second string
 * @return -1 if first string is before s2 in the alphabetical order, else 1 and 0 if the two s1 and s2 are the same.
 */
bool compareByAlphabetical( const string& s1, const string& s2 ) {
    return strcasecmp( s1.c_str(), s2.c_str() ) <= EQ;
}

/**
 * send a message containing the connected clients.
 * @param sender_sd the sender socket discreptor.
 */
void who(int sender_sd)
{
    vector<string> connected_clients;
    string conn_clients;
    for(map<string,int>::iterator it = clientNames.begin(); it != clientNames.end();it++)
    {
        connected_clients.push_back(it->first);
    }
    sort(connected_clients.begin(),connected_clients.end(),compareByAlphabetical);
    for(vector<string>::iterator it = connected_clients.begin(); it!=connected_clients.end(); it++)
    {
        conn_clients=conn_clients + *it +",";
    }
    conn_clients.erase(conn_clients.size() - 1);
    conn_clients = conn_clients + ".";
    send(sender_sd,conn_clients.c_str(),strlen(conn_clients.c_str()), SEND_FLAG);

}

/**
 * this function is called whenever a client has been disconnected.
 * @param sender_sd the sd of the client who wants to exit.
 */
void client_disconnected(int sender_sd) {
    for (map<string, int>::iterator it = clientNames.begin(); it != clientNames.end(); it++)
    {
        if (it->second == sender_sd)
        {
            clientNames.erase(it);
        }
    }
    for (std::map<string,vector<int>>::iterator it = clientGroups.begin(); it!=clientGroups.end(); it++)
    {
        for(vector<int> ::iterator it2 = it->second.begin(); it2!=it->second.end();it2++)
        {
            if(*it2 == sender_sd)
            {
                it->second.erase(it2);
            }
        }

    }

    string exit_msg = "Unregistered successfully.\n";
    send(sender_sd,exit_msg.c_str(),exit_msg.length()+1, SEND_FLAG);



}

/**
 * this function is called whenever a client has been connected to the server.
 * @param clientName the name of the joined client.
 * @param sd the sd of the connected client
 */
void client_connect(char* clientName, int sd)
{

    string name(clientName);
    clientNames[name] = sd;

}

/**
 * a function that checks if the given client is connected to the server.
 * @param clientName the name of the client to be checked.
 * @return 1 if the client is connected, 0 otherwise.
 */
int isClientConn(char* clientName)
{
    for(map<string,int>::iterator it = clientNames.begin(); it != clientNames.end(); it ++)
    {
        if(strcmp(it->first.c_str() ,clientName)== EQ)
            return 1;
    }

    for(map<string,vector<int>>::iterator it2 = clientGroups.begin(); it2!= clientGroups.end(); it2 ++)
    {
        if(strcmp(it2->first.c_str() ,clientName)== EQ)
            return 1;
    }
    return 0;
}

/**
 * a function that checks if there is a group with the name groupName
 * @param groupName the group name to be check.
 * @return 1 if the group exist, else 0.
 */
int is_group_exist(char* groupName)
{
    for (std::map<string,vector<int>>::iterator it = clientGroups.begin(); it!=clientGroups.end(); it++)
    {
        if(strcmp(it->first.c_str(),groupName) == EQ)
        {
            return 1;
        }
    }
    return 0;

}




/**
 * parser for the client command.
 * @param message the command of the client.
 * @param sender_sd the sd of the command sender.
 * @return 1 if the command is legal and was treated well, -1 if and error happened, -2 in case that the client exit.
 */
int command_parser(char* message, int sender_sd) {

    char *token = strtok(message, " ");
    string quote = "\"";
    if (strcmp(token, "join") == EQ){
        token = strtok(NULL," ");

        if(isClientConn(token) )
        {

            string str = "client already connected";
            send(sender_sd,str.c_str(),str.size(), SEND_FLAG);
            return -1 ;
        }
        client_connect(token,sender_sd);
        cout << token<< " connected."<<endl;
    }
    else if (strcmp(token, "create_group") == SEND_FLAG)
    {
        vector<int> group_friends_sd;
        group_friends_sd.push_back(sender_sd);
        char group_name[256]  ;
        token = strtok(NULL," ");
        strcpy(group_name, token);
        string err = "ERROR: failed to create group "+quote+group_name+quote+".";
        if(isClientConn(group_name) || is_group_exist(group_name))
        {
            string quote = "\"";

            cout<<get_clientName(sender_sd)<<":"<<err<<endl;

            send(sender_sd,err.c_str(),err.size(), SEND_FLAG);
            return -1;
        }
        token = strtok(NULL, ",");
        if(token == NULL){
            cout<<get_clientName(sender_sd)<<":"<<err<<endl;
            send(sender_sd,err.c_str(),err.size(), SEND_FLAG);
            return -1;
        } else{
            string string1 = token;
            if (string1.find(" ") != std::string::npos) {
                cout<<get_clientName(sender_sd)<<":"<<err<<endl;
                send(sender_sd,err.c_str(),err.size(), SEND_FLAG);
                return -1;
            }
        }

        while (token != NULL)
        {

            if(isClientConn(token))
            {
                int client_sd = clientNames[token];
                if(find(group_friends_sd.begin(),group_friends_sd.end(),client_sd) == group_friends_sd.end())
                {
                    group_friends_sd.push_back(clientNames[token]);
                }

            }
            else
            {
                cout<<get_clientName(sender_sd)<<":"<<err<<endl;
                send(sender_sd,err.c_str(),err.size(), SEND_FLAG);
                return -1;
            }
            token = strtok(NULL, ",");
        }

        if(group_friends_sd.size() < 2)
        {
            cout<<get_clientName(sender_sd)<<":"<<err<<endl;
            send(sender_sd,err.c_str(),err.size(), SEND_FLAG);
            return -1;
        }

        string str = string(group_name);


        str =  "Group " +quote + str + quote + " was created successfully." ;
        create_group(group_name,group_friends_sd);
        send(sender_sd , str.c_str() , str.size() , SEND_FLAG);
        string clientName = get_clientName(sender_sd);
        cout << clientName+": "<< str<<endl;


    }
    else if (strcmp(token, "send") == EQ)
    {
        char  receiver [30];
        token = strtok(NULL," ");
        strcpy(receiver, token);

        char msg [256];
        token = strtok(NULL,"");
        strcpy(msg, token);
        if(is_group_exist(receiver))
        {
            if(find(clientGroups[receiver].begin(),clientGroups[receiver].end(),sender_sd) != clientGroups[receiver].end()){
                for(vector<int>::iterator it = clientGroups[receiver].begin(); it!=clientGroups[receiver].end(); it++)
                {

                    if(*it != sender_sd)
                    {
                        sendmsg(*it,msg,sender_sd,true);
                    }
                }
                cout << get_clientName(sender_sd) <<": "<<quote<<msg<<quote<<" was sent successfully to "<<receiver <<"."<<endl;
                string s = "Sent successfully.";
                send(sender_sd,s.c_str(),s.size(), SEND_FLAG);
            } else{
                string s = "ERROR: failed to send.";
                cout << get_clientName(sender_sd) <<": ERROR: failed to send "<<quote << msg << quote << " to "<<receiver <<"."<<endl;
                send(sender_sd,s.c_str(),s.size(), SEND_FLAG);
                return -1;
            }

        }
        else if (isClientConn(receiver))
        {
            if(clientNames[receiver] != sender_sd)
            {
                sendmsg(clientNames[receiver],msg,sender_sd,false);
                string s = "Sent successfully.";
                send(sender_sd,s.c_str(),s.size(), SEND_FLAG);

            }
            else
            {
                cout << get_clientName(sender_sd) <<": ERROR: failed to send "<<quote << msg << quote << " to "<<receiver <<"."<<endl;
                string s = "ERROR: failed to send.";
                send(sender_sd,s.c_str(),s.size(), SEND_FLAG);
                return -1;
            }
        }
        else
        {
            string s = "ERROR: failed to send.";
            cout << get_clientName(sender_sd) <<": ERROR: failed to send "<<quote << msg << quote << " to "<<receiver <<"."<<endl;
            send(sender_sd,s.c_str(),s.size(), SEND_FLAG);
            return -1;
        }
    }
    else if (strcmp(token, "who") == EQ)
    {
        who(sender_sd);
        cout<< get_clientName(sender_sd)<<": Requests the currently connected client names."<<endl;
    }
    else if(strcmp(token, "exit") == EQ)
    {

        cout << get_clientName(sender_sd)<<": Unregistered successfully."<<endl;
        client_disconnected(sender_sd);

        return -2;
    }



}

int main_work()
{
    int PORT = atoi(argv1[1]);

    int opt = TRUE;
    int  addrlen , new_socket  , activity, i , valread , sd;
    int max_sd;


    char buffer[1025];

    string welcome_message = "Connected successfully.";

    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
    {
        cerr << "ERROR: socket " << master_socket <<"."<<endl;
        exit(EXIT_FAILURE);
    }


    if(int er = setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                   sizeof(opt)) < 0 )
    {
        cerr << "ERROR: setsockopt " << er <<"."<<endl;
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    if (int er = bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        cerr << "ERROR: bind " << er <<"."<<endl;
        exit(EXIT_FAILURE);
    }

    if (int er = listen(master_socket, 3) < 0)
    {
        cerr << "ERROR: listen " << er <<"."<<endl;
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);


    while(TRUE)
    {
        FD_ZERO(&readfds);

        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for ( i = 0 ; i < max_clients ; i++)
        {
            sd = client_socket[i];

            if(sd > 0)
            {

                FD_SET( sd , &readfds);

            }

            if(sd > max_sd)
                max_sd = sd;
        }

        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR))
        {
            cout<<"select error"<<endl;
        }


        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                cerr << "ERROR: accept " << new_socket <<"."<<endl;
                exit(EXIT_FAILURE);
            }
            read( new_socket , buffer, 256);
            int retVal = command_parser(buffer,new_socket);

            if(retVal == -1 )
            {
                continue;
            }

            if( int er = send(new_socket, welcome_message.c_str(), welcome_message.size(), SEND_FLAG) != welcome_message.size() )
            {
                cerr << "ERROR: send " << er <<"."<<endl;
            }

            for (i = 0; i < max_clients; i++)
            {
                if( client_socket[i] == EQ )
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        for (i = 0; i < max_clients; i++)
        {
            char buf[256];
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds))
            {
                if ((valread = read( sd , buf, 1024)) != EQ)
                {

                    int res = command_parser(buf, sd);
                    if (res == -2)
                    {
                        getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                        close( sd );
                        client_socket[i] = 0;
                    }

                }
            }
        }
    }

    return 0;
}

void write_msg(){
    while(true){
        string command;
        getline(std::cin, command);
        if (strcmp(command.c_str(),"EXIT") == EQ)
        {
            for (int i = 0; i < max_clients; i++)
            {
                string exit_msg = "SERVER EXIT.\n";
                send(client_socket[i],exit_msg.c_str(),exit_msg.length()+1, SEND_FLAG);
            }
            exit(0);
        }

    }
}

int main(int argc , char *argv[])
{
    argv1 = argv;
    thread thread1 = thread(main_work);
    thread thread2 = thread(write_msg);
    thread1.join();
    thread2.join();
}



