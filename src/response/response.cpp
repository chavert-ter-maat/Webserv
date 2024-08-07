#include "response.hpp"
#include "cgi.hpp"
#include "defines.hpp"
#include "dir_listing.hpp"
#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

Response::Response():
	_request(nullptr), _contentType(""), _body(""), _contentLength(0), _responseString(""),
	_fileAccess(nullptr), _finalPath(""), _cgi(nullptr), _complete(true)
{}

Response::Response(std::shared_ptr<Request> request, std::list<ServerStruct> *config, int port):
	_request(request), _contentType(""), _body(""), _contentLength(0), _responseString(""),
	_fileAccess(config), _finalPath(""), _cgi(nullptr), _complete(true)
{
	int return_code = 0;

	_finalPath = _request->get_requestPath();

	if (!_finalPath.empty() && _finalPath.string()[0] == '/')
		_finalPath = _finalPath.string().substr(1);

	_finalPath = _fileAccess.isFilePermissioned( _finalPath, return_code, port, _request->get_requestMethod());
	if (return_code == 301)
	{
		buildResponse(static_cast<int>(return_code), redirect(_fileAccess.get_return()), false);
	}
	else if (return_code) {
		std::cout << return_code << "Path error:" << _finalPath << std::endl;
		// _finalPath = _fileAccess.getErrorPage(return_code); // wrong place
		_body = get_error_body(return_code, "Not Found.");
		buildResponse(static_cast<int>(return_code), "Not Found", false);
	}
	else {
		handleRequest(request);
	}
	#ifdef DEBUG
	printResponse();
	#endif
}

Response::~Response() {}

Response::Response(const Response &src):
	_request(src._request), _contentType(src._contentType), _body(src._body), _contentLength(src._contentLength),
	_responseString(src._responseString), _fileAccess(src._fileAccess),
	_finalPath(src._finalPath), _cgi(src._cgi), _complete(src._complete)
{}

Response &Response::operator=(const Response &rhs)
{
	Response temp(rhs);
	temp.swap(*this);
	return *this;
}

void Response::swap(Response &lhs)
{
	std::swap(_request, lhs._request);
	std::swap(_contentType, lhs._contentType);
	std::swap(_body, lhs._body);
	std::swap(_contentLength, lhs._contentLength);
	std::swap(_responseString, lhs._responseString);
	std::swap(_fileAccess, lhs._fileAccess);
	std::swap(_finalPath, lhs._finalPath);
	std::swap(_cgi, lhs._cgi);
	std::swap(_complete, lhs._complete);
}

std::string	Response::get_error_body(int error_code, std::string error_description)
{
	std::string				error_body;
	std::filesystem::path	error_page;

	error_page = _fileAccess.getErrorPage(error_code);
	if (error_page != "")
		error_body = readFileToBody(error_page);
	else
		error_body = standard_error(error_code, error_description);
	return (error_body);
}

void Response::handleRequest(const std::shared_ptr<Request> &request)
{
	if (_request && !_request->isValid()) {
		buildResponse(static_cast<int>(statusCode::BAD_REQUEST), "Bad Request");
		return;
	}
	std::string request_method = request->get_requestMethod();
	try {
		if (request_method == "GET" && _fileAccess.allowedMethod("GET"))
			handleGetRequest(request);
		else if (request_method == "POST" && _fileAccess.allowedMethod("POST"))
			handlePostRequest(request);
		else if (request_method == "DELETE" && _fileAccess.allowedMethod("DELETE"))
			handleDeleteRequest(request);
		else
		{
			_body = get_error_body(static_cast<int>(statusCode::METHOD_NOT_ALLOWED), "Method not allowed.");
			buildResponse(static_cast<int>(statusCode::METHOD_NOT_ALLOWED),
						"Method Not Allowed", false);
		}
	}
	catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		buildResponse(static_cast<int>(statusCode::INTERNAL_SERVER_ERROR),
					"Internal Server Error", false);
	}
}

bool Response::handleGetRequest(const std::shared_ptr<Request> &request) {
	std::stringstream buffer;

	if (!_finalPath.empty() && _finalPath.has_extension())
	{
		std::unordered_map<std::string, std::string>::const_iterator it =
			contentTypes.find(_finalPath.extension());
		if (it == contentTypes.end())
		{
			_body = get_error_body(static_cast<int>(statusCode::OK), "Unsupported Media Type");
			buildResponse(static_cast<int>(statusCode::UNSUPPORTED_MEDIA_TYPE),
				  "Unsupported Media Type", false);
			return false;
		}
		_contentType = it->second;
		if (interpreters.find(_finalPath.extension()) == interpreters.end())
		{
			_body = readFileToBody(_finalPath);
			if (_body.empty())
			{
				std::cout << "empty body" << std::endl;
				_body = get_error_body(404, "File not found.");
				buildResponse(static_cast<int>(statusCode::NOT_FOUND), "Not Found", false);
				return true;
			}
		}
		else
		{
			_cgi = std::make_shared<CGI>(_request, _finalPath, interpreters.at(_finalPath.extension()));
			_complete = _cgi->isComplete();
			if (_complete == true) {
				_body = _cgi->get_result();
				_contentLength = _cgi->get_contentLength();
				buildResponse(static_cast<int>(statusCode::OK), "OK", true);
			}
			return true;
		}
	}
	else
		_body = list_dir(_finalPath, request->get_requestPath(), request->get_referer());
	buildResponse(static_cast<int>(statusCode::OK), "OK", false);
	return true;
}

bool Response::handlePostRequest(const std::shared_ptr<Request> &request)
{
	std::string requestBody = request->get_body();
	std::string requestContentType = request->get_contentType();
	bool isCGI = false;

	if (!_finalPath.empty() && _finalPath.string()[0] == '/')
		_finalPath = _finalPath.string().substr(1);

	if (_finalPath.has_extension()) {
		if (interpreters.find(_finalPath.extension()) != interpreters.end()) {
			isCGI = true;
			_cgi = std::make_shared<CGI>(_request, _finalPath, interpreters.at(_finalPath.extension()));
			_complete = _cgi->isComplete();
			if (_complete == true) {
				_body = _cgi->get_result();
				_contentLength = _cgi->get_contentLength();
				buildResponse(static_cast<int>(statusCode::OK), "OK", isCGI);
			}
			return true;
		}
		else {
			buildResponse(static_cast<int>(statusCode::NO_CONTENT), "No Content", "");
			return true;
		}
	}
	if (requestContentType == "multipart/form-data") {
		handle_multipart();
		return true;
	}
	buildResponse(static_cast<int>(statusCode::OK), "OK", isCGI);
	return true;
}

bool Response::handleDeleteRequest(const std::shared_ptr<Request> &request)
{
	std::cout << request->get_requestPath() << std::endl;
	if (_fileAccess.is_deleteable(_finalPath))
	{
		std::cout << _finalPath << std::endl;
		if (remove(_finalPath))
		{
			buildResponse(static_cast<int>(statusCode::OK), "OK", "");
			return true;
		}
	}
	buildResponse(static_cast<int>(204), "Failed", ""); //change this untill correct
	return false;
}

const std::string	Response::readFileToBody(std::filesystem::path path)
{
	std::stringstream buffer;
	std::string body;
	std::ifstream file( path, std::ios::binary);

	if (!file) {
		std::cout << "Error, invalid path: " << path << std::endl;
		return "";
	}
	buffer << file.rdbuf();
	body = buffer.str();
	return body;
}

void Response::handle_multipart()
{
	statusCode	status = statusCode::OK;
	std::string	requestBody = _request->get_body();
	std::string	boundary = _request->get_boundary();
	std::string	filename = "";
	bool		append = false;

	std::vector<std::string> parts = split_multipart(requestBody, boundary);
	for (const std::string &part: parts) {
		if (part.empty())
			continue;

		size_t header_end = part.find(CRLFCRLF);
		if (header_end == std::string::npos)
			continue;

		std::string headers = part.substr(0, header_end);
		std::string content = part.substr(header_end + 4);
		
		std::transform(headers.begin(), headers.end(), headers.begin(), [](unsigned char c){ return std::tolower(c);} );

		size_t contentType = headers.find("content-type:");
		if (contentType == std::string::npos) {
			continue;
		}

		filename = extract_filename(headers);
		status = write_file(      _finalPath.string() + "/" + filename, content, append); //TODO:make customizable via config -> _finalPath.string()
		if (status != statusCode::OK)
			break;
		append = true;
		#ifdef DEBUG
		std::cout << "File Upload success!" << std::endl;
		std::cout << MSG_BORDER << MSG_BORDER << std::endl;
		#endif // DEBUG
		if (status == statusCode::OK)
			_body = list_dir(_finalPath, _request->get_requestPath(), _request->get_referer());//readFileToBody("html/upload_success.html");
		else
			_body = get_error_body(static_cast<int>(status), "File not found.");//readFileToBody("html/standard_404.html");
	}
	buildResponse(static_cast<int>(status), statusCodeMap.at(status), false);
}

std::vector<std::string> Response::split_multipart(std::string requestBody, std::string boundary)
{
	std::vector<std::string> parts;
	std::string fullBoundary = "--" + boundary;
	size_t pos = 0;

	while (pos < requestBody.size()) {
		size_t start = requestBody.find(fullBoundary, pos);
		if (start == std::string::npos)
			break;
		size_t end = requestBody.find(fullBoundary, start + fullBoundary.length());
		if (end  == std::string::npos)
			end = requestBody.size();
		parts.push_back(requestBody.substr(start + fullBoundary.length(), end - start - fullBoundary.length() - 2));
		pos = end;
	}
	return parts;
}

std::string Response::extract_filename(const std::string &headers)
{
	size_t filename_pos = headers.find("filename=\"");
				//
	if (filename_pos == std::string::npos)
				return "temp.txt";
	size_t filename_end = headers.find("\"", filename_pos + 10);
	return headers.substr(filename_pos + 10, filename_end - filename_pos - 10);
}

statusCode Response::write_file(const std::string &path, const std::string &content, bool append)
{
	std::ofstream file;
	if (append)
		file.open(path, std::ios::binary | std::ios::app);
	else
		file.open(path, std::ios::binary);
	if (file.is_open()) {
		file.write(content.c_str(), content.length());
		file.close();
		return statusCode::OK;
	}
	else
		return statusCode::INTERNAL_SERVER_ERROR;
}

void	Response::continue_cgi()
{
	if (_cgi->readCGIfd()) {
		std::cerr << "cgi reading error" << std::endl;
		buildResponse(static_cast<int>(statusCode::INTERNAL_SERVER_ERROR), "Internal Server Error", false); // when cgi double padded?
		_complete = true;
		return;
	}
	if (_cgi->isComplete() == true) {
		_body = _cgi->get_result();
		_contentLength = _cgi->get_contentLength();
		buildResponse(static_cast<int>(statusCode::OK), "OK", true); // when cgi double padded?
		_complete = true;
	}
}

void Response::buildResponse(int status, const std::string &message, bool isCGI)
{
	_responseString.append("HTTP/1.1 " + std::to_string(status) + " " + message +
							CRLF);
	if (isCGI) {
		_responseString.append("Content-Length: " + std::to_string(_contentLength) +
							CRLF);
		_responseString.append(_body);
	}
	else {
		if (_body.empty()) {
			return;
		}
		else {
			_responseString.append("Content-Length: " + std::to_string(_body.length()) +
						  CRLF);
			_responseString.append("Content-Type: " + get_contentType() + CRLF);
			_responseString.append(CRLF + _body);
		}
	}
}

const std::string	&Response::get_response() const { return _responseString; }
const std::string	&Response::get_contentType() const { return _contentType; }
bool		Response::isComplete() const { return _complete; }

void Response::printResponse() {
	std::cout << MSG_BORDER << "[Response]" << MSG_BORDER << std::endl;
	std::cout << _responseString << std::endl;
}
