#pragma once

#define MSG_BORDER "---------------------------"

#define GET_EXAMPLE                                                            \
  "GET /index.html HTTP/1.1\r\n\
Host: www.example.com\r\n\
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n\
Accept-Language: en-US,en;q=0.5\r\n\
Accept-Encoding: gzip, deflate\r\n\
Connection: keep-alive\r\n\r\n"

#define POST_EXAMPLE                                                           \
  "POST /submit-form HTTP/1.1\r\n\
Host: www.example.com\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Content-Length: 31\r\n\r\n\
username=johndoe&password=12345"

#define INVALID_EXAMPLE                                                        \
  "Host: www.example.com\r\n\
POST /submit-form HTTP/1.1\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Content-Length: 27\r\n\r\n\
username=johndoe&password=12345"
