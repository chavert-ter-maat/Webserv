#include "fileAccess.hpp"
#include "defines.hpp"

int	max_body_limit(std::list<ServerStruct> *config, int port)
{
	std::string	port_str;

	port_str = std::to_string(port);
	for (ServerStruct &server_config : *config)
	{
		for (std::string &port_config : server_config._port.content_list)
		{
			if (port_config == port_str)
			{
				if (!server_config._clientMaxBodySize.content_list.empty())
					return (std::stoull(server_config._clientMaxBodySize.content_list.front()));
				else
					return (DEFUALT_CLIENT_MAX_BODY_SIZE);
			}
		}
	}
	return (DEFUALT_CLIENT_MAX_BODY_SIZE);
}

FileAccess::FileAccess(std::list<ServerStruct> *config): config(config)
{
	_return = "";
	_clientMaxBodySize = DEFUALT_CLIENT_MAX_BODY_SIZE;
}

FileAccess::~FileAccess() {}

bool	find_server_name_in_uri(std::string host, std::string server_name)
{
	host = host.substr(0, host.find(":"));
	if (server_name.at(0) == '*' && host.find(server_name.substr(1), (host.length() - server_name.length())) == host.length() - server_name.length() + 1)
		return (true);
	else if (server_name.at(server_name.length() - 1) == '*' && !host.find(server_name.substr(0, server_name.length() - 1)))
		return (true);
	else if (!host.find(server_name) && host.length() == server_name.length())
		return (true);
	return (false);
}

std::string	remove_server_name(std::string uri, std::string server_name)
{
	std::string	new_uri;

	if (server_name.at(0) == '*' && uri.find(server_name.substr(1), (uri.length() - server_name.length())) == uri.length() - server_name.length() + 1)
		new_uri = uri.substr(0, uri.length() - server_name.length());
	else if (server_name.at(server_name.length() - 1) == '*' && !uri.find(server_name.substr(0, server_name.length() - 1)))
		new_uri = uri.substr(server_name.length() - 1);
	else if (!uri.find(server_name))
		new_uri = uri.substr(server_name.length());
	else
		new_uri = uri;
	if (new_uri.length() > 1 && new_uri.at(0) == '/')
		new_uri = new_uri.substr(1);
	return (new_uri);
}

std::string	FileAccess::swap_to_right_server_config(std::string uri, int port, std::string host)
{
	std::string		port_str;
	ServerStruct	*prev_match;

	prev_match = NULL;
	port_str = std::to_string(port);
	for (ServerStruct &server_config : *config)
	{
		for (std::string &port_config : server_config._port.content_list)
		{
			if (port_config == port_str)
			{
				if (!server_config._names.content_list.empty())
				{
					for (std::string &server_name : server_config._names.content_list)
					{
						if (find_server_name_in_uri(host, server_name))
						{
							if (server_name.length() > _prevServerName.length())
							{
								prev_match = &server_config;
								_prevServerName = server_name;
							}
						}
						else if (!prev_match)
						{
							prev_match = &server_config;
							_prevServerName = "";
						}
					}
				}
				else if (!prev_match)
				{
					prev_match = &server_config;
					_prevServerName = "";
				}
			}
		}
	}
	server = prev_match;
	_root = server->_root.content_list.back();
	_currentRoot = _root;
	_allowedMethods = &server->_allowMethods.content_list;
	_currentAllowedMethods = _allowedMethods;
	return (uri);
}

#define MATCH_TYPE_UNSPECIFIED 0
#define MATCH_TYPE_EXACT 1
#define MATCH_TYPE_POSTFIX 2

int	get_match_type(std::string first_content_part)
{
	if (!first_content_part.compare("="))
		return (MATCH_TYPE_EXACT);
	else if (!first_content_part.compare("*="))
		return (MATCH_TYPE_POSTFIX);
	else
		return (MATCH_TYPE_UNSPECIFIED);
}

bool			FileAccess::is_deleteable(std::filesystem::path to_delete)
{
	std::filesystem::path root;
	std::filesystem::path current_root;

	root = _root;
	current_root = _currentRoot;
	if (to_delete == "")
		return (false);
	root = std::filesystem::absolute(root);
	current_root = std::filesystem::absolute(current_root);
	to_delete = std::filesystem::absolute(to_delete);
	if (!std::filesystem::exists(to_delete))
		return false;
	to_delete = std::filesystem::canonical(to_delete);
	if (!std::filesystem::exists(current_root))
	{
		current_root = std::filesystem::canonical(current_root);
		if (to_delete.string().find(current_root.string()) == 0 && to_delete.string().find(current_root.string()) == 0)
			return true;
	}
	root = std::filesystem::canonical(root);
	if (to_delete.string().find(root.string()) == 0 && to_delete.string().find(root.string()) == 0)
		return true;
	return false;
}

bool	method_in_location(ConfigContent *current, ConfigContent *parent, std::string method)
{
	std::list<std::string>	*allowedMethods;

	if ((LocationStruct *)current->childs && !((LocationStruct *)current->childs)->allow_methods.content_list.empty())
		allowedMethods = &((LocationStruct *)current->childs)->allow_methods.content_list;
	else
		allowedMethods = &((LocationStruct *)parent->childs)->allow_methods.content_list;
	for (std::string content : *allowedMethods)
	{
		if (!content.compare(method))
			return true;
	}
	return false;
}

ConfigContent	*FileAccess::find_location_config(std::string uri, ConfigContent *location_config, std::string method)
{
	ConfigContent	*current;
	ConfigContent	*previous_match;
	int				match_type_requested;
	std::string		*loc_conf;
	std::string		new_uri;

	current = location_config;
	previous_match = NULL;
	new_uri = "/";
	new_uri.append(uri);
	uri = new_uri;
	while (current)
	{
		match_type_requested = get_match_type(current->content_list.front());
		loc_conf = &current->content_list.back();
		if (match_type_requested != MATCH_TYPE_POSTFIX 
			&& !uri.find(*loc_conf)
			&& uri.length() >= loc_conf->length())
		{
			if (match_type_requested == MATCH_TYPE_EXACT && !uri.compare(*loc_conf))
			{
				previous_match = current;
				break;
			}
			else
			{
				if (!previous_match || (previous_match->content_list.back().length() < loc_conf->length()
					&& method_in_location(current, &server->_location, method)))
					previous_match = current;
			}
		}
		else if (match_type_requested == MATCH_TYPE_POSTFIX)
		{
			if (uri.find(*loc_conf, (uri.length() - loc_conf->length())) == uri.length() - loc_conf->length()
				&& method_in_location(current, &server->_location, method))
			{
				previous_match = current;
				break;
			}
		}
		current = current->next;
	}
	if (previous_match && (LocationStruct *)previous_match->childs && !((LocationStruct *)previous_match->childs)->allow_methods.content_list.empty())
		_currentAllowedMethods = &((LocationStruct *)previous_match->childs)->allow_methods.content_list;
	return (previous_match);
}

std::string	FileAccess::swap_out_root(std::string uri, ConfigContent *location_config, std::string root)
{
	std::filesystem::path	root_swapped_path;
	std::filesystem::path	path;

	if (location_config->childs && !((LocationStruct *)location_config->childs)->root.content_list.empty())
		root = ((LocationStruct *)location_config->childs)->root.content_list.back();
	root_swapped_path = root;
	if (!uri.empty())
	{
		path = uri;
		if (location_config->content_list.back() != "/" && get_match_type(location_config->content_list.front()) != MATCH_TYPE_POSTFIX
			&& (location_config->content_list.back().length() <= path.parent_path().string().length() + 1 || path.extension() == ""))
		{
			if (location_config->content_list.back().length() <= uri.length())
				root_swapped_path.append(uri.substr(location_config->content_list.back().length()));
		}
		else
			root_swapped_path.append(uri);
	}
	return (root_swapped_path);
}

std::filesystem::path FileAccess::uri_is_directory(std::string uri, ConfigContent *location_config, int &return_code)
{
	std::filesystem::path	path;

	path = uri;
	if (!path.extension().empty())
		return (path);
	if (location_config && location_config->childs)
	{
		if (!((LocationStruct *)location_config->childs)->index.content_list.empty())
		{
			path.append(((LocationStruct *)location_config->childs)->index.content_list.back());
			return (path);
		}
		if (!((LocationStruct *)location_config->childs)->autoindex.content_list.back().compare("on"))
			return (path);
	}
	return_code = 404;
	return ("");	
}

std::string	FileAccess::redirect(int &return_code)
{
	if (!server->_return.content_list.empty())
	{
		return_code = 301;
		_return = server->_return.content_list.back();
		return ("");
	}
	return ("");
}

std::filesystem::path	FileAccess::isFilePermissioned(std::string uri, int &return_code, int port, std::string method, std::string host)
{
	std::string		new_uri;
	ConfigContent	*location_config;
	std::filesystem::path	path;


	uri = swap_to_right_server_config(uri, port, host);
	new_uri = redirect(return_code);
	if (return_code == 301)
		return ("/");
	if (!server->_clientMaxBodySize.content_list.empty())
		_clientMaxBodySize = std::stoull(server->_clientMaxBodySize.content_list.front());
	location_config = &server->_location;
	location_config = find_location_config(uri, location_config, method);
	if (location_config && location_config->childs && !((LocationStruct *)location_config->childs)->allow_methods.content_list.empty())
		_currentAllowedMethods = &((LocationStruct *)location_config->childs)->allow_methods.content_list;
	else if (!location_config)
	{
		return_code = 404;
		return ("");
	}
	else
		_currentAllowedMethods = _allowedMethods;
	new_uri = swap_out_root(uri, location_config, _root);
	path = uri_is_directory(new_uri, location_config, return_code);
	if (return_code)
		return (path);
	if (is_deleteable(path))
		return (path);
	else
		return ("");
}

std::filesystem::path	FileAccess::getErrorPage(int return_code)
{
	ConfigContent	*current;
	std::string		error_code;

	error_code = std::to_string(return_code);
	current = &server->_errorPage;
	while (current)
	{
		if (!current->content_list.front().compare(error_code))
			return (current->content_list.back());
		current = current->next;
	}
	return ("");
}

bool	FileAccess::allowedMethod(std::string method)
{
	for (std::string content : *_currentAllowedMethods)
	{
		if (!content.compare(method))
			return true;
	}
	return false;
}

std::string	FileAccess::get_return(void)
{
	return (_return);
}
