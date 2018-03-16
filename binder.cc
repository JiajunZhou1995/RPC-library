#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <list>
#include <cstring>
#include <netinet/in.h>
#include <map>
#include "common.h"
#include <iostream>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>

bool terminate;

int sendMessage(int socket_fd, MessageType msg_type, char msg_data[])
{
    send(socket_fd, &msg_type, sizeof(MessageType), 0);
    if (msg_type == LOC_FAILURE)
    {
        send(socket_fd, msg_data, 4, 0);
    }
    else if (msg_type == LOC_SUCCESS)
    {
        send(socket_fd, msg_data, 4 + 128, 0);
    }
    return 0;
}

struct Funcition
{
    std::string name;
    int *argTypes;
    int argSize;
    Funcition(std::string name, int *argTypes, int argSize) : name(name), argTypes(argTypes), argSize(argSize) {}
};

struct Server
{
    char *serverId;
    int port;
    int socketFd;
    Server(char *serverId, int port, int socketFd) : serverId(serverId), port(port), socketFd(socketFd) {}
};

std::list<Server *> serverQueue;
std::map<Funcition *, std::list<Server *> > funcDict;

void registerMyServer(char *serverId, unsigned short port, int socketFd)
{
    Server *location = new Server(serverId, port, socketFd);

    for (std::list<Server *>::iterator it = serverQueue.begin(); it != serverQueue.end(); ++it)
    {
        if (*it == location)
        {
            return;
        }
    }
    serverQueue.push_back(location);
}

void registerMyFunction(std::string name, int *argTypes, int argSize, char *serverId, int port, int socketFd)
{
    bool found = false;
    Server *location = new Server(serverId, port, socketFd);
    Funcition *func = new Funcition(name, argTypes, argSize / 4);
    for (std::map<Funcition *, std::list<Server *> >::iterator it = funcDict.begin(); it != funcDict.end(); it++)
    {
        if (func->name == it->first->name && func->argSize == it->first->argSize)
        {
            found = true;
            for (int i = 0; i < func->argSize; i++)
            {
                if (func->argTypes[i] != it->first->argTypes[i])
                {
                    found = false;
                    break;
                }
            }
        }

        if (found)
        {
            for (std::list<Server *>::iterator listit = it->second.begin(); listit != it->second.end(); ++listit)
            {
                if (*listit == location)
                {
                    return;
                }
            }
            it->second.push_back(location);
        }
    }

    if (!found)
    {
        funcDict[func].push_back(location);
    }
    registerMyServer(serverId, port, socketFd);
    return;
}

void handleRegReq(int clientSocketFd)
{
    int reason;

    int serverlength;
    int status = recv(clientSocketFd, &serverlength, sizeof(int), 0);
    char serverId[128];
    recv(clientSocketFd, serverId, serverlength * sizeof(char), 0);
    char *buffer = new char[128];
    memcpy(buffer, serverId, 128 * sizeof(char));
    int port;
    recv(clientSocketFd, &port, sizeof(int), 0);

    int funcNamelength;
    recv(clientSocketFd, &funcNamelength, sizeof(int), 0);
    char funcName[funcNamelength];
    recv(clientSocketFd, &funcName, funcNamelength * sizeof(char), 0);

    int argTypelength;
    recv(clientSocketFd, &argTypelength, sizeof(int), 0);
    int *argType = new int[argTypelength];
    int argSize = (argTypelength - 1) * sizeof(int);
    recv(clientSocketFd, &argType, argSize, 0);

    std::string name(funcName);

    char responseMsg[sizeof(int)];
    registerMyFunction(name, argType, argSize, buffer, port, clientSocketFd);

    sendMessage(clientSocketFd, REGISTER_SUCCESS, NULL);
}

Server *lookupServer(std::string name, int *argTypes, int argSize)
{
    Server *selectedServer = NULL;
    Funcition *func = new Funcition(name, argTypes, argSize / 4);
    bool argmatch = false;
    for (std::map<Funcition *, std::list<Server *> >::iterator it = funcDict.begin(); it != funcDict.end(); it++)
    {

        if (func->name == it->first->name)
        {
            argmatch = true;
        }

        if (argmatch)
        {
            std::list<Server *> availServers = it->second;
            for (int i = 0; i < serverQueue.size(); i++)
            {
                Server *server = serverQueue.front();
                for (std::list<Server *>::iterator listit = availServers.begin(); listit != availServers.end(); ++listit)
                {
                    if (server->serverId == (*listit)->serverId && server->port == (*listit)->port && server->socketFd == (*listit)->socketFd)
                    {
                        selectedServer = server;
                        break;
                    }
                }
                serverQueue.pop_front();
                serverQueue.push_back(server);
            }
        }
    }

    return selectedServer;
}

void handleLocReq(int clientSocketFd)
{
    char funcName[64];
    recv(clientSocketFd, &funcName, 64 * sizeof(char), 0);

    int argTypelength;
    recv(clientSocketFd, &argTypelength, sizeof(int), 0);
    int *argType = new int[argTypelength];
    ;
    recv(clientSocketFd, argType, argTypelength * sizeof(int), 0);
    int argSize = argTypelength * sizeof(int);

    std::string name(funcName);

    Server *availServer = lookupServer(name, argType, argSize);

    if (!availServer)
    {
        char responseMsg[sizeof(int)];
        int reason = 1;
        memcpy(responseMsg, &reason, sizeof(int));
        sendMessage(clientSocketFd, LOC_FAILURE, responseMsg);
    }
    else
    {
        char responseMsg[128 + 4];
        memcpy(responseMsg, availServer->serverId, 128);
        memcpy(responseMsg + 128, &(availServer->port), 4);
        sendMessage(clientSocketFd, LOC_SUCCESS, responseMsg);
    }
}

int handleRequest_helper_length(int clientSocketFd, fd_set *mainFds)
{
    int msgLength;
    if (read(clientSocketFd, &msgLength, 4) <= 0)
    {
        close(clientSocketFd);
        FD_CLR(clientSocketFd, mainFds);
        return -10;
    }
    return msgLength;
}

void handleRequest(int clientSocketFd, fd_set *mainFds)
{
    MessageType msgType;
    if (read(clientSocketFd, &msgType, 4) <= 0)
    {
        close(clientSocketFd);
        FD_CLR(clientSocketFd, mainFds);
        return;
    }

    if (msgType == REGISTER)
    {
        handleRegReq(clientSocketFd);
    }
    else if (msgType == LOC_REQUEST)
    {
        handleLocReq(clientSocketFd);
    }
    else if (msgType == TERMINATE)
    {
        for (std::list<Server *>::iterator it = serverQueue.begin(); it != serverQueue.end(); ++it)
        {
            sendMessage((*it)->socketFd, TERMINATE, NULL);
        }
        terminate = true;
    }
}

int main()
{
    terminate = false;
    struct sockaddr_in serverAdd, clientAdd;
    fd_set mainFds;
    fd_set curFds;

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0)
    {
        std::cerr << "Error: cannot open a socket" << std::endl;
        return 1;
    }

    memset((char *)&serverAdd, 0, sizeof(serverAdd));

    serverAdd.sin_family = AF_INET;

    serverAdd.sin_addr.s_addr = htons(INADDR_ANY);

    serverAdd.sin_port = htons(0);

    bind(socketFd, (struct sockaddr *)&serverAdd, sizeof(serverAdd));
    listen(socketFd, 5);

    socklen_t size = sizeof(serverAdd);
    char server_name[128];
    getsockname(socketFd, (struct sockaddr *)&serverAdd, &size);
    gethostname(server_name, 128);

    std::cout << "BINDER_ADDRESS " << server_name << std::endl;
    std::cout << "BINDER_PORT " << ntohs(serverAdd.sin_port) << std::endl;

    FD_ZERO(&mainFds);
    FD_SET(socketFd, &mainFds);
    int count = socketFd;

    while (1)
    {
        if (terminate)
            exit(0);
        curFds = mainFds;
        select(count + 1, &curFds, NULL, NULL, NULL);

        for (int i = 0; i <= count; i++)
        {
            if (FD_ISSET(i, &curFds))
            {
                if (i != socketFd)
                {
                    int clientSocketFd = i;
                    handleRequest(clientSocketFd, &mainFds);
                }
                else
                {
                    socklen_t size = sizeof(clientAdd);
                    int newFd = accept(socketFd, (struct sockaddr *)&clientAdd, &size);
                    if (newFd < 0)
                    {
                        std::cerr << "Error: cannot establish new connection" << std::endl;
                        return 1;
                    }
                    FD_SET(newFd, &mainFds);
                    if (newFd > count)
                    {
                        count = newFd;
                    }
                }
            }
        }
    }
}
