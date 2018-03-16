
#include <string>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include "common.h"
#include "rpc.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <pthread.h>
#include <vector>
#include <queue>
#include <cstring>
#include <assert.h>
#include <stdio.h>
#include <sstream>
#include <list>

int terminate_request = -1;
int server_fd = -1;
int binder_fd = -1;
char * binder_port = getenv("BINDER_PORT");
const char* binder_hostname = getenv("BINDER_ADDRESS");
struct input
{
	int input_sock;
	int input_port;
	std::list<std::string> *input_buffer;

	input()
	{
		input_sock = 0;
		input_port = 0;
		input_buffer = NULL;
	}
};

struct Procedure
{	int arg_len;
    char *name;
    int *argTypes;
};
std::vector<Procedure *> func_container;
std::vector<skeleton> skels;
// std::queue<pthread_t *> execute_children;
int connectOrExit(int sockfd, struct sockaddr_in address, int size)
{
	if (connect(sockfd, (struct sockaddr *)&address, size) < 0)
	{
		//std::cout << "ERROR: Failed to connecting socket" << std::endl;
		return FAIL_TO_CONNECT;
	}
	return COMPLETE;
}


// given the arg type, return the length of arg
int get_size(int* arg_type)
{
    // The length is the last two values
    int length = (*arg_type) & 0x0000FFFF;
    if(length == 0)
        return 1;
    return length;
}

// given the arg type, return the type of arg
int receive_type(int* arg_type)
{
    // Second byte is the type
    int type = (*arg_type) & 0x00FF0000;
    return type >> 16;
}

// given the int type value, return the size of type
int malloc_size(int type)
{
    if(type == ARG_CHAR)
        return sizeof(char);
    else if(type == ARG_SHORT)
        return sizeof(short);
    else if(type == ARG_INT)
        return sizeof(int);
    else if(type == ARG_LONG)
        return sizeof(long);
    else if(type == ARG_DOUBLE)
        return sizeof(double);
    else if(type == ARG_FLOAT)
        return sizeof(float);
    return -1;
}

int connectIt(int port, char *binder_host){
	struct input td;
	struct sockaddr_in address;
	struct hostent *server;
	pthread_t thread;
	std::string data;
	std::list<std::string> buffer;

	if (binder_host == NULL)
	{
		//std::cout << "ERROR: SERVER_ADDRESS and SERVER_PORT environment variables are not seted" << std::endl;
		exit(EXIT_FAILURE);
	}

	std::string serverAddress = std::string(binder_host);
	//int port = atoi(getenv("SERVER_PORT"));

	td.input_buffer = &buffer;
	td.input_port = port;
	td.input_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (td.input_sock < 0)
	{
		//std::cout << "ERROR: Failed to creat a socket: " << td.input_sock << std::endl;
		//exit(EXIT_FAILURE);
		return INVALID_FD;
	}

	server = gethostbyname(serverAddress.c_str());

	memset((char *)&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	memcpy((char *)server->h_addr, (char *)&address.sin_addr.s_addr, server->h_length);
	address.sin_port = htons(td.input_port);
	connectOrExit(td.input_sock, address, sizeof(address));
	return td.input_sock;

}

int connect_to(int port, const char* binder_host) {
	struct hostent *host;
	if (binder_host == NULL) {
		return HOSTNAME_ERROR;
	}
	host = gethostbyname(binder_host);
	if (host == NULL) {
		return HOSTNAME_ERROR;
	}
	char * ip;
	ip = inet_ntoa(*((struct in_addr *)host->h_addr_list[0]));
	if (ip == NULL) {
		return FAIL_TO_CONNECT;
	}
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return INVALID_FD;
	}
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1){
		close(fd);
		return FAIL_TO_CONNECT;
	}
	return fd;
}

int rpcInit(void) {
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sock;
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = INADDR_ANY;
	sock.sin_port = htons(0);
	int ret = bind(server_fd,(struct sockaddr*) &sock, sizeof(sock));
	if (ret < 0) {
		close(server_fd);
		return FAIL_TO_BIND;
	}
	ret = listen(server_fd, 10);
	if (ret < 0) {
		close(server_fd);
		return FAIL_TO_LISTEN;
	}
	if (binder_port == NULL) {
		return INVALID_PORT;
	}
	binder_fd = connect_to(atol(binder_port),binder_hostname);
	if (binder_fd < 0) {
		return binder_fd;
	}
	return COMPLETE;
}


int checkSkeleton(skeleton f) {
	for (unsigned i = 0; i < skels.size(); i++) {
		if (skels[i] == f) {
			return SKELETON_HAS_ALREADY_REGISTERED;
		} 
	}
	return COMPLETE;
} 


int sendRegisterInfo(char *name, int* argTypes) {
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(server_fd, (struct sockaddr *)&sin, &len);
	size_t host_size = 128;
	char hostname[host_size];
	gethostname(hostname, host_size);
	// std::cout << "Server HOSTNAME: " << std::string(hostname) << std::endl;
	int port = ntohs(sin.sin_port);
    int ip_len = 128;
    int i;
    // count argType length except the last item
    for (i = 0; argTypes[i] != 0; i++)
    {
    }
    int name_len = 64;
    //int total_len = sizeof(int) + sizeof(MessageType) + ip_len * sizeof(char) + sizeof(int) + name_len * sizeof(char) + i * sizeof(int);
    // send total length
    //send(binder_fd, &total_len, sizeof(int), 0);
    MessageType r = REGISTER;
	// send REGISTER
	send(binder_fd,&r,sizeof(MessageType),0);
	// send ip length and hostname
    
	send(binder_fd, &ip_len ,sizeof(int),0);
	send(binder_fd, hostname ,ip_len * sizeof(char),0);
	// send  port number
	send(binder_fd, &port,sizeof(int),0);
	// send name length and name
	send(binder_fd,&name_len,sizeof(int),0);
	send(binder_fd, name ,name_len,0);
	

	// send length of argtypes and argtypes
	send(binder_fd,&i,sizeof(int), 0);
	send(binder_fd, argTypes, i * sizeof(int), 0);
	// receive messageTpe returned from binder
	recv(binder_fd,&r,sizeof(int),0);
	//recv(binder_fd,&i,sizeof(int),0);
	if (r == REGISTER_SUCCESS) {
		return COMPLETE;
	} else {
		return FAIL_TO_REGISTER;
	}
	
	// returnValue could be 0, positive for warning or negative for errors

	
}
int addProcedure(char *name, int* argTypes, skeleton f) {
	int len = 0;
	for (len = 0; argTypes[len] != 0;len++) {

	}
	struct Procedure *new_proc = (struct Procedure *)malloc(sizeof(struct Procedure));
	new_proc->arg_len = len-1;
	new_proc->argTypes = argTypes;
	new_proc->name = name;
	assert(func_container.size() == skels.size());
	func_container.push_back(new_proc);
	skels.push_back(f);
	return COMPLETE;
}
int updateProcedure(char *name, int* argTypes, skeleton f){
	for (unsigned i = 0; i < func_container.size(); i++) {
		struct Procedure * p = func_container[i];
		if (p->name == name) {
			std::vector<int> types;
			unsigned j;
			for (unsigned j = 0; argTypes[j] != 0 && p->argTypes[j] != 0; i++) {
				if (argTypes[j] != p->argTypes[j]) {
					break;
				}
				
			}
			if (p->argTypes[j] == 0 && argTypes[j] == 0) {
				if (j == 0 || argTypes[j-1] == p->argTypes[j-1]) {
					skels.erase(skels.begin()+i);
					skels.insert(skels.begin()+i,f);
					return COMPLETE;
				}
			}
			
		}
	}
	return SKELETON_NOT_FOUND;
}

int recvRequestFromBinder(int binder, int * port, char * hostname) {
	MessageType answer;
	recv(binder,&answer,sizeof(MessageType),0);
	if (answer == LOC_SUCCESS) {
		int ip_len = 128;
		//recv(binder,&ip_len,sizeof(int),0);
		recv(binder,hostname,ip_len * sizeof(char),0);
		// port number
		recv(binder, port, sizeof(int), 0);
		//close(binder);
		return 0;
	} else if (answer == LOC_FAILURE) {
		int reason;
		recv(binder,&reason,sizeof(int),0);

		//close(binder);
		return reason;
	} else {

		//close(binder);
		return UNEXPECTED_MESSAGE;
	}
}
// void send_name_and_argtypes_from_client(int fd, int len, MessageType request, char * name, int * argTypes) {
// 	send(fd,&request,sizeof(MessageType),0);
// 	int func_len = 64;
// 	// name 
// 	send(fd,&func_len,sizeof(int),0);
// 	send(fd,name,func_len * sizeof(char),0);
// 	send(fd,&len,sizeof(int),0);
// 	send(fd,argTypes,len * sizeof(int),0);
// }
// void receive_name_argtype_and_args(int fd, int *len, char *name, int *argtypes, void **args)
// {
//     recv(fd, name, 64 * sizeof(char), 0);
//     recv(fd, &len, sizeof(int), 0);
//     recv(fd, argtypes, (*len) * sizeof(int), 0);
//     *len -= 1;
//     for (int i = 0; i < *len; i++)
//     {
//         int arg_len, arg_tp, arg_size;
//         recv(fd, &arg_len, sizeof(int), 0);
//         recv(fd, &arg_tp, sizeof(int), 0);
//         arg_size = arg_len * arg_tp;
//         *(args + i) = malloc(arg_size);
//         recv(fd, *(args + i), arg_size, 0);
//     }
// }

int rpcCall(char* name, int* argTypes, void** args) {
    if (binder_port == NULL)
    {
        return INVALID_PORT;
    }
    int binder = connect_to(atol(binder_port),binder_hostname);
	if (binder < 0) {
		return binder;
	}
    unsigned i;
    int argType_length;
    for (i = 0; argTypes[i] != 0; i++) {}
    argType_length = i;
	MessageType request = LOC_REQUEST;
	//send_name_and_argtypes_from_client(binder, argType_length, request, name, argTypes);
	// int name_len;
	// int argType_length;
	send(binder,&request,sizeof(MessageType),0);
	// name 
	send(binder,name,64 * sizeof(char),0);
	send(binder,&argType_length,sizeof(int),0);
	send(binder,argTypes,argType_length * sizeof(int),0);
	char *ip = new char[128];
	int port;
	int ret = recvRequestFromBinder(binder,&port,ip);
	if (ret < 0) {
		// LOC_FAILURE
		return ret;
	}
	int to_server = connectIt(port, ip);
	if (to_server < 0) {
		return to_server;
	}
	request = EXECUTE;
	send(to_server,&request,sizeof(MessageType),0);
	int func_len = 64;
	// name
	send(to_server,name,func_len * sizeof(char),0);
	send(to_server,&argType_length,sizeof(int),0);
	send(to_server, argTypes, argType_length * sizeof(int), 0);
	int total_size = 0;
	for (int i = 0; i < argType_length; i++) {
		// int arg_tp = (argTypes[i] >> 8) & 0xff; // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		// int arg_len = argTypes[i] & 0x0000FFFF;  // !!!
		int arg_type = receive_type(&argTypes[i]);
		int arg_length = get_size(&argTypes[i]);
		int arg_size = malloc_size(arg_type);
		int size = arg_length * arg_size;
		send(to_server,&arg_length,sizeof(int),0);
		send(to_server,&arg_size,sizeof(int),0);
		//send(to_server, &size, sizeof(int), 0);
		send(to_server, (void*) args[i], size, 0);
	}
	// receive part 
	int len;
	MessageType answer;
	char func_name[64];
	recv(to_server,&answer,sizeof(MessageType),0);
	int reason = UNEXPECTED_MESSAGE;
	if (answer == EXECUTE_FAILURE){
		recv(to_server, &reason, sizeof(int), 0);
	} else if (answer == EXECUTE_SUCCESS){
		recv(to_server, func_name, 64 * sizeof(char), 0);
		recv(to_server, &len, sizeof(int), 0);
		int argtypes[len];
		recv(to_server, argtypes, len * sizeof(int), 0);
		void *func_args[len];
		for (int i = 0; i < len; i++)
		{
			int arg_len, arg_tp, arg_size;
			recv(to_server, &arg_len, sizeof(int), 0);
			recv(to_server, &arg_tp, sizeof(int), 0);
			arg_size = arg_len * arg_tp;
			recv(to_server, args[i], arg_size, 0);
		}
		reason = COMPLETE;
	}
	return reason;
}



int rpcCacheCall(char* name, int* argTypes, void** args){
	return COMPLETE;
}

int rpcRegister(char *name, int* argTypes, skeleton f) {
	if (binder_fd < 0 || server_fd < 0) {
		return NOT_INIT;
	}
    assert(binder_port);
	int fd = connect_to(atol(binder_port),binder_hostname);
	if (fd < 0) {
		return fd;
	}
	int ret = sendRegisterInfo(name,argTypes);
	if (ret >= 0) {
		int s = updateProcedure(name,argTypes,f);
		if (s > 0) {
			addProcedure(name,argTypes,f);
		}
	}
	return ret;
}

void * receive_termination(void* fd) {
	int sock = *((int *) fd);
	assert(sock >= 0);
	MessageType ret;
	while (true) {
		recv(sock,&ret,sizeof(MessageType),0);
		if (ret == TERMINATE) {
			terminate_request = 1;
			break;
		}
	}
	pthread_exit(NULL);
}

int look_for_matched_skeleton(char *given_name, int *argtypes)
{
    int ret = -1;
    for (unsigned i = 0; i < func_container.size(); i++)
    {
        bool found = true;
        char *name = (func_container.at(i))->name;
        int *types = (func_container.at(i))->argTypes;
        if (strcmp(given_name, name) == 0)
        {
            for (unsigned j = 0; argtypes[j] != 0 && types[j] != 0; j++)
            {
                if (argtypes[j] != types[j])
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                return i;
            }
        }
    }
    return ret;
}

void * execute(void* sock) {
	int fd = *((int *) sock);
	MessageType msg_type;
	recv(fd,&msg_type,sizeof(MessageType),0);
	if (msg_type == EXECUTE) {
		// receive 
		int length;
		char *name = new char[64];
        // receive_name_argtype_and_args(fd, &len, name, argtypes, args);
		recv(fd, name, 64 * sizeof(char), 0);
		recv(fd, &length, sizeof(int), 0);
		int * argtypes = new int[length];
		int *argtypes_pointer = new int[length];
		recv(fd, argtypes, length * sizeof(int), 0);
		memcpy(argtypes_pointer, argtypes, length * sizeof(int));
		void **args = (void **)malloc(length * sizeof(char *));
		void * argsLocation = argtypes_pointer + length;

		for (int i = 0; i < length; i++)
		{
			int arg_size, arg_len;
			recv(fd, &arg_len, sizeof(int), 0);
			// recv(fd, &arg_tp, sizeof(int), 0);
			// arg_size = arg_len * arg_tp;
			recv(fd,&arg_size, sizeof(int),0);
			if (arg_size == 0) {
				arg_size = 1;
			}
			int sizeInByte = arg_len * arg_size;
			args[i] = (char *)malloc(sizeInByte);
			recv(fd, args[i], sizeInByte, 0);
		}
		// find skeleton
		ErrorMsg returnValue;
		MessageType return_type;
		bool send_back = false;
		int index = look_for_matched_skeleton(name,argtypes);
		if (index >= 0 && index < skels.size()) {
			//skeleton * f = & (skels.at(index));
            returnValue = (ErrorMsg)(skels.at(index))(argtypes, (void**)args);
            if (!returnValue) {
				return_type = EXECUTE_SUCCESS;
				// SUCCESS
				send(fd,&return_type,sizeof(int),0);
				//  name
				send(fd,name,64*sizeof(char),0);
				// argtypes_len and argtypes
				send(fd,&length,sizeof(int),0);
				send(fd,argtypes,length*sizeof(int),0);
				int total_length = 0;
				for (unsigned k = 0; k < length; k++) {
					int arg_type = receive_type(&argtypes[k]);
					int arg_length = get_size(&argtypes[k]);
					int arg_size = malloc_size(arg_type);
					int size = arg_length * arg_size;
					send(fd,&arg_length,sizeof(int),0);
					send(fd,&arg_size,sizeof(int),0);
					send(fd, (void*) args[k], size, 0);
					//std::cout << "Output: " << *(int*)args[k] << std::endl;
				}
				send_back = true;
			}
		} 
		if (!send_back) {
			return_type = EXECUTE_FAILURE;
			send(fd, &return_type,sizeof(int),0);
			send(fd,&returnValue,sizeof(int),0);
		}
	}
	close(fd);
	pthread_exit(NULL);
}
int rpcExecute(void) {
	if (server_fd < 0 || binder_fd < 0) {
		return NOT_INIT;
	}
	if (skels.size() == 0) {
		return NOT_REGISTER;
	}
    pthread_t terminate_thread;
    pthread_create(&terminate_thread, NULL, receive_termination,(void *) &binder_fd);
	while (terminate_request < 0) {
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		int fd = accept(server_fd,(struct sockaddr *) &addr, &addr_len);
		if (fd >= 0) {
			pthread_t child;
			pthread_create(&child, NULL, execute, (void *)&fd);
		} else if (terminate_request < 0) {
			break;
		}
	}
	close(server_fd);
	return COMPLETE;

}

int rpcTerminate(void) {
	if (binder_fd < 0 || server_fd < 0) {
		return NOT_INIT;
	} else {
		MessageType m = TERMINATE;
		send(binder_fd,&m,sizeof(MessageType),0);
		close(binder_fd);
		return COMPLETE;
	}
}


