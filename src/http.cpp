#include "http.h"

#include <ctype.h>

const int http::err_codes[HTTP_ERR_SIZE] = {
    [HTTP_ERR_200] = 200,
    [HTTP_ERR_400] = 400,
    [HTTP_ERR_401] = 401,
    [HTTP_ERR_403] = 403,
    [HTTP_ERR_404] = 404,
    [HTTP_ERR_405] = 405,
    [HTTP_ERR_407] = 407,
    [HTTP_ERR_408] = 408,
    [HTTP_ERR_410] = 410,
    [HTTP_ERR_413] = 413,
    [HTTP_ERR_414] = 414,
    [HTTP_ERR_421] = 421,
    [HTTP_ERR_422] = 422,
    [HTTP_ERR_425] = 425,
    [HTTP_ERR_429] = 429,
    [HTTP_ERR_500] = 500,
    [HTTP_ERR_501] = 501,
    [HTTP_ERR_502] = 502,
    [HTTP_ERR_503] = 503,
    [HTTP_ERR_504] = 504,
    [HTTP_ERR_505] = 505,
};

const char *http::err_msgs[HTTP_ERR_SIZE] = {
    [HTTP_ERR_200] =
        "HTTP/1.1 200 OK\r\n"
        "Content-length: 58\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>200 OK</h1>\nService ready.\n</body></html>\n",

    [HTTP_ERR_400] =
        "HTTP/1.1 400 Bad request\r\n"
        "Content-length: 90\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>400 Bad request</h1>\nYour browser sent an invalid request.\n</body></html>\n",

    [HTTP_ERR_401] =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-length: 112\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>401 Unauthorized</h1>\nYou need a valid user and password to access this content.\n</body></html>\n",

    [HTTP_ERR_403] =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-length: 93\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>403 Forbidden</h1>\nRequest forbidden by administrative rules.\n</body></html>\n",

    [HTTP_ERR_404] =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-length: 83\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>404 Not Found</h1>\nThe resource could not be found.\n</body></html>\n",

    [HTTP_ERR_405] =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Content-length: 146\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>405 Method Not Allowed</h1>\nA request was made of a resource using a request method not supported by that resource\n</body></html>\n",

    [HTTP_ERR_407] =
        "HTTP/1.1 407 Unauthorized\r\n"
        "Content-length: 112\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>407 Unauthorized</h1>\nYou need a valid user and password to access this content.\n</body></html>\n",

    [HTTP_ERR_408] =
        "HTTP/1.1 408 Request Time-out\r\n"
        "Content-length: 110\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>408 Request Time-out</h1>\nYour browser didn't send a complete request in time.\n</body></html>\n",

    [HTTP_ERR_410] =
        "HTTP/1.1 410 Gone\r\n"
        "Content-length: 114\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>410 Gone</h1>\nThe resource is no longer available and will not be available again.\n</body></html>\n",

    [HTTP_ERR_413] =
        "HTTP/1.1 413 Payload Too Large\r\n"
        "Content-length: 106\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>413 Payload Too Large</h1>\nThe request entity exceeds the maximum allowed.\n</body></html>\n",

    [HTTP_ERR_414] =
        "HTTP/1.1 414 Request-URI Too Long\r\n"
        "Content-length: 132\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>414 Request-URI Too Long</h1>\nThe requested URL's length exceeds the capacity limit for this server.\n</body></html>\n",

    [HTTP_ERR_421] =
        "HTTP/1.1 421 Misdirected Request\r\n"
        "Content-length: 104\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>421 Misdirected Request</h1>\nRequest sent to a non-authoritative server.\n</body></html>\n",

    [HTTP_ERR_422] =
        "HTTP/1.1 422 Unprocessable Content\r\n"
        "Content-length: 116\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>422 Unprocessable Content</h1>\nThe server cannot process the contained instructions.\n</body></html>\n",

    [HTTP_ERR_425] =
        "HTTP/1.1 425 Too Early\r\n"
        "Content-length: 80\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>425 Too Early</h1>\nYour browser sent early data.\n</body></html>\n",

    [HTTP_ERR_429] =
        "HTTP/1.1 429 Too Many Requests\r\n"
        "Content-length: 117\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>429 Too Many Requests</h1>\nYou have sent too many requests in a given amount of time.\n</body></html>\n",

    [HTTP_ERR_500] =
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-length: 97\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>500 Internal Server Error</h1>\nAn internal server error occurred.\n</body></html>\n",

    [HTTP_ERR_501] =
        "HTTP/1.1 501 Not Implemented\r\n"
        "Content-length: 136\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>501 Not Implemented</h1>\n.The server does not support the functionality required to fulfill the request.\n</body></html>\n",

    [HTTP_ERR_502] =
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-length: 107\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>502 Bad Gateway</h1>\nThe server returned an invalid or incomplete response.\n</body></html>\n",

    [HTTP_ERR_503] =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-length: 107\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>503 Service Unavailable</h1>\nNo server is available to handle this request.\n</body></html>\n",

    [HTTP_ERR_504] =
        "HTTP/1.1 504 Gateway Time-out\r\n"
        "Content-length: 92\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>504 Gateway Time-out</h1>\nThe server didn't respond in time.\n</body></html>\n",

    [HTTP_ERR_505] =
        "HTTP/1.1 505 HTTP Version Not Supported\r\n"
        "Content-length: 67\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body><h1>505 HTTP Version Not Supported</h1>\n</body></html>\n",
};

const char *http::get_reason(const int status) {
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 102: return "Processing";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 210: return "Content Different";
    case 226: return "IM Used";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 310: return "Too many Redirects";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Time-out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large"; // Content/Payload Too Large
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested range unsatisfiable";
    case 417: return "Expectation failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Content";
    case 423: return "Locked";
    case 424: return "Method failure";
    case 425: return "Too Early";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 449: return "Retry With";
    case 450: return "Blocked by Windows Parental Controls";
    case 451: return "Unavailable For Legal Reasons";
    case 456: return "Unrecoverable Error";
    case 499: return "client has closed connection";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway or Proxy Error";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Time-out";
    case 505: return "HTTP Version not supported";
    case 506: return "Variant also negotiate";
    case 507: return "Insufficient storage";
    case 508: return "Loop detected";
    case 509: return "Bandwidth Limit Exceeded";
    case 510: return "Not extended";
    case 511: return "Network authentication required";
    case 520: return "Web server is returning an unknown error";
    default:
        switch (status) {
        case 100 ... 199: return "Informational";
        case 200 ... 299: return "Success";
        case 300 ... 399: return "Redirection";
        case 400 ... 499: return "Client Error";
        case 500 ... 599: return "Server Error";
        default:          return "Other";
        }
    }
}
static int htoi(char *s) {
    int value = 0;
    int c = 0;

    c = ((unsigned char *)s)[0];
    if (isupper(c))
        c = tolower(c);
    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

    c = ((unsigned char *)s)[1];
    if (isupper(c))
        c = tolower(c);
    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

    return value;
}
int http::url_decode(char *str, int len) {
    char *dest = str;
    char *data = str;

    while (len--) {
        if (*data == '+')
            *dest = ' ';
        else if (*data == '%' && len >= 2
            && isxdigit((int) *(data + 1))
            && isxdigit((int) *(data + 2))) {
            *dest = (char)htoi(data + 1);
            data += 2;
            len -= 2;
        } else
            *dest = *data;

        data++;
        dest++;
    }
    *dest = '\0';
    return dest - str;
}
