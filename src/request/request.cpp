#include "request.hpp"
#include "defines.h"
#include "stringUtils.hpp"
#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

Request::Request() : _rawRequest(""), _isValid(0) {}

auto print_key_value = [](const auto &key, const auto &value) {
  std::cout << "Key:[" << key << "] Value:[" << value << "]\n";
};

Request::Request(const std::string &rawStr) : _rawRequest(rawStr) {
  parseRequest();
  printRequest();
}

Request::~Request() {}

Request::Request(const Request &src)
    : _rawRequest(src._rawRequest), _requestMethod(src._requestMethod),
      _uri(src._uri), _htmlVersion(src._htmlVersion),
      _requestArgs(src._requestArgs), _headers(src._headers),
      _keepAlive(src._keepAlive), _body(src._body), _isValid(src._isValid) {
  extractCgiEnv();
}

Request &Request::operator=(const Request &rhs) {
  Request temp(rhs);
  temp.swap(*this);
  return *this;
}

void Request::swap(Request &lhs) {
  std::swap(_requestMethod, lhs._requestMethod);
  std::swap(_uri, lhs._uri);
  std::swap(_requestArgs, lhs._requestArgs);
  std::swap(_htmlVersion, lhs._htmlVersion);
  std::swap(_headers, lhs._headers);
  std::swap(_keepAlive, lhs._keepAlive);
  std::swap(_body, lhs._body);
  std::swap(_isValid, lhs._isValid);
}

// doesn't seem to be the standard but it is an security issue not to check
// this, https://www.htmlhelp.com/faq/cgifaq.2.html
// https://datatracker.ietf.org/doc/html/rfc3875#section-4
void Request::extractCgiEnv() // whoops wrong place, also security issue :p
{
  if (get_headers().empty())
    return;
  // Should check for accepted env variables, but dont think its specified to be
  // that way.
  for (const auto &[key, value] : get_headers())
    print_key_value(key, value);
}

void Request::parseRequest() {
  std::string line;
  std::string headerKey;
  std::string headerValue;

  if (_rawRequest.empty()) {
    _isValid = false;
    return;
  }

  std::istringstream requestStream(_rawRequest);
  std::getline(requestStream, line, '\r');
  requestStream.get();

  if (!parseRequestLine(line)) {
    _isValid = false;
    return;
  }

  _uri = trim(_uri, "/");
  _requestArgs = parse_requestArgs(_uri);

  if (!parseRequestHeaders(requestStream)) {
    _isValid = false;
    return;
  }

  parseRequestBody(_rawRequest);

  if (_headers.find("connection") != _headers.end()) {
    if (_headers["connection"].compare("keep-alive") == 0) {
      _keepAlive = true;
    }
  }

  _isValid = checkRequestValidity();
  return;
}

std::vector<std::string> Request::parse_requestArgs(const std::string uri) {
  size_t pos;
  std::vector<std::string> args;
  std::string argStr;

  pos = uri.find("?");
  if (pos != std::string::npos) {
    _uri = uri.substr(0, pos);
    if (pos + 1) {
      argStr = uri.substr(pos + 1);
      args = split(argStr, "?");
    }
  }
  return args;
}

bool Request::parseRequestLine(const std::string &line) {
  std::istringstream lineStream(line);
  if (!(lineStream >> _requestMethod >> _uri >> _htmlVersion))
    return false;
  return true;
}

bool Request::parseRequestHeaders(std::istringstream &requestStream) {
  std::string line;
  size_t pos;
  size_t headerPos;
  std::string headerKey;
  std::string headerValue;

  while (std::getline(requestStream, line) && !line.empty() && line != "\r") {
    pos = line.find(":");
    if (pos != std::string::npos) {
      headerPos = pos + 1;
      while (line[headerPos] == ' ')
        headerPos++;

      headerKey = line.substr(0, pos);
      for (auto &c : headerKey)
        c = tolower(c);

      headerValue =
          line.substr(headerPos, line.find_last_not_of("/r") - headerPos);
      for (auto &c : headerValue)
        c = tolower(c);
      _headers[headerKey] = headerValue;
    }
  }
  return true;
}

bool Request::parseRequestBody(const std::string &_rawRequest) {
  size_t body_start;

  body_start = _rawRequest.find("\r\n\r\n");
  if (body_start == std::string::npos) {
    return false;
  }
  _body = _rawRequest.substr(body_start + 4, get_contentLen());
  return true;
}

// TODO: max length of GET request 2048 bytes?
bool Request::checkRequestValidity() const {
  if (_requestMethod.empty() || _htmlVersion.empty() || _uri.empty())
    return false;
  if (_requestMethod != "GET" && _requestMethod != "POST" &&
      _requestMethod != "DELETE")
    return false;
  if (_htmlVersion != "HTTP/1.0" && _htmlVersion != "HTTP/1.1" &&
      _htmlVersion != "HTTP/2.0")
    return false;
  if (_htmlVersion != "HTTP/1.0" && _headers.find("host") == _headers.end())
    return false;
  return true;
}

const std::string &Request::get_rawRequest() const { return _rawRequest; }

const std::string &Request::get_requestMethod() const { return _requestMethod; }

const std::string &Request::get_uri() const { return _uri; }

const std::string Request::get_referer() const {
  auto referer = _headers.find("referer");
  if (referer == _headers.end())
    return "";
  return referer->second;
}

const std::string Request::get_contentType() const {
  auto res = _headers.find("content-type");
  if (res == _headers.end())
    return "";
  std::string contentType = res->second;
  size_t pos = contentType.find(";");
  if (pos != std::string::npos) {
    contentType = contentType.substr(0, pos);
  }
  return contentType;
}

size_t Request::get_contentLen() const {
  auto contentLenStr = _headers.find("content-length");
  if (contentLenStr == _headers.end())
    return 0;

  try {
    size_t contentLen = std::stoul(contentLenStr->second);
    return contentLen;
  } catch (const std::invalid_argument &e) {
    return 0;
  } catch (const std::out_of_range &e) {
    return 0;
  }
}

const std::string Request::get_boundary() const {
  std::string ret;

  auto boundary = _headers.find("content-type");
  if (boundary != _headers.end()) {
    ret = boundary->second.substr(boundary->second.find("boundary=") + 9);
  } else {
    ret = "";
  }
  return ret;
}

const std::string &Request::get_body() const { return _body; }

const bool &Request::get_keepAlive() const { return _keepAlive; }

const std::string &Request::get_htmlVersion() const { return _htmlVersion; }

const std::vector<std::string> &Request::get_requestArgs() const {
  return _requestArgs;
}

const std::unordered_map<std::string, std::string> &
Request::get_headers() const {
  return _headers;
}

const bool &Request::get_validity() const { return _isValid; }

void Request::printRequest() const {
  std::cout << MSG_BORDER << "[Request]" << MSG_BORDER << std::endl;
  std::cout << get_rawRequest() << std::endl;

#ifdef DEBUG
  std::cout << MSG_BORDER << "[Parsed Request]" << MSG_BORDER << std::endl;
  std::cout << "request method: " << get_requestMethod() << std::endl;
  std::cout << "uri: " << get_uri() << std::endl;
  std::cout << "html version: " << get_htmlVersion() << std::endl;

  std::cout << "<URI Args>" << std::endl;
  for (auto it : _requestArgs) {
    std::cout << it << std::endl;
  }

  std::cout << "<Headers>" << std::endl;
  std::unordered_map<std::string, std::string> headers = get_headers();
  for (auto it : headers) {
    std::cout << it.first << ": " << it.second << std::endl;
  }

  if (get_requestMethod() == "POST") {
    std::cout << "<POST Header Getters>" << std::endl;
    std::cout << "Referer: " << get_referer() << std::endl;
    std::cout << "ContentType: " << get_contentType() << std::endl;
    std::cout << "Boundary: " << get_boundary() << std::endl;
    std::cout << "ContentLen: " << get_contentLen() << std::endl;
  }

  std::cout << "<Keep-Alive>" << std::endl;
  bool keepAlive = get_keepAlive();
  (keepAlive ? std::cout << "Keep-Alive: true" << std::endl
             : std::cout << "Keep-Alive: false" << std::endl);

  std::cout << "<Body>" << std::endl;
  std::string body = get_body();
  (body.empty()) ? std::cout << "Empty Body" << std::endl
                 : std::cout << body << std::endl;

  std::cout << "<Request Validity>" << std::endl;
  (checkRequestValidity() == true) ? (std::cout << "Valid!" << std::endl)
                                   : (std::cout << "Invalid!" << std::endl);
#endif // DEBUG
}
