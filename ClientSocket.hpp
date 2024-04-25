#ifndef CLIENTSOCKET_HPP
#define CLIENTSOCKET_HPP

#include <iostream>
#include <vector>
#include <unistd.h>
#include <cstring>
#include <sys/poll.h>
#include <unistd.h>
#include "ServerSocket.hpp"

class ClientSocket : ServerSocket{
	private:
		std::vector<int> 	_connectedClientSockets;
		std::vector<pollfd>	pfds;

	public:
		ClientSocket();
		~ClientSocket();

		void	handleInputEvent(int index);
		void	handleIncomingConnection(int index, int serverSocket_fd);
		void	addClientSocketToFds(int client_fd);
		void	startPolling(int server_fd);
};

#endif