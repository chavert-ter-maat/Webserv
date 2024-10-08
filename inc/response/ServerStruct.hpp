#pragma once

#include <vector>
#include "sys/poll.h"
#include "ProtoStruct.hpp"
#include "LocationStruct.hpp"

class ServerStruct: public ProtoStruct
{
	private:
	ParserStruct	*_source;

	public:
	int				_serverNum;
	std::string		_id;
	ConfigContent	_port;
	ConfigContent	_host;
	ConfigContent	_names;
	ConfigContent	_root;
	ConfigContent	_location;
	ConfigContent	_errorPage; 
	ConfigContent	_return; 
	ConfigContent	_allowMethods;
	ConfigContent	_clientMaxBodySize;


	ServerStruct();
	ServerStruct(ParserStruct *parser_struct, int nth_server);
	ServerStruct(const ServerStruct &src);
	~ServerStruct();
	ServerStruct	&operator=(const ServerStruct &to_copy);
	void			show_self(void);
};

int	load_in_servers(ParserStruct *PS, std::list<ServerStruct> &server_structs);
int	double_ports(std::list<ServerStruct> &server_structs);
