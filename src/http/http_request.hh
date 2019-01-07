#pragma once

#include <buffer.hh>
#include <util_string.hh>

#include <string>
#include <cstring>
#include <map>
#include <memory>

namespace eve
{
enum http_cmd_type
{
    REQ_GET,
    REQ_POST,
    REQ_HEAD
};
enum http_request_kind
{
    REQUEST,
    RESPONSE
};

enum message_read_status
{
    ALL_DATA_READ = 1,
    MORE_DATA_EXPECTED = 0,
    DATA_CORRUPTED = -1,
    REQUEST_CANCELED = -2
};

/* Response codes */
#define HTTP_OK 200
#define HTTP_NOCONTENT 204
#define HTTP_MOVEPERM 301
#define HTTP_MOVETEMP 302
#define HTTP_NOTMODIFIED 304
#define HTTP_BADREQUEST 400
#define HTTP_NOTFOUND 404
#define HTTP_SERVUNAVAIL 503

class http_connection;
class http_request
{
  public:
    buffer *input_buffer = NULL;
    buffer *output_buffer = NULL;
    /* the connection object that this request belongs to */
    http_connection *conn = NULL;
    int flags;
#define REQ_OWN_CONNECTION 0x0001
#define PROXY_REQUEST 0x0002

    /* address of the remote host and the port connection came from */
    std::string remote_host;
    unsigned short remote_port;

    enum http_request_kind kind;
    enum http_cmd_type type;
    std::string uri; /* uri after HTTP request was parsed */
    unsigned short major = 1;      /* HTTP Major number */
    unsigned short minor = 1;      /* HTTP Minor number */

    int response_code;              /* HTTP Response code */
    std::string response_code_line; /* Readable response */

    bool handled = false;

    long ntoread;
    int chunked = 0;

    void (*cb)(std::shared_ptr<http_request>) = NULL;
    // void (*chunk_cb)(std::shared_ptr<http_request>);

    std::map<std::string, std::string> input_headers;
    std::map<std::string, std::string> output_headers;

  public:
    http_request();
    ~http_request();

    inline void set_response(int code, const std::string &reason)
    {
        this->kind = RESPONSE;
        this->response_code = code;
        this->response_code_line = reason;
    }

    inline void set_cb(void (*cb)(std::shared_ptr<http_request>)) { this->cb = cb; }

    inline int is_connection_keepalive()
    {
        std::string connection = input_headers["Connection"];
        return (!connection.empty() && iequals_n(connection, "keep-alive", 10));
    }

    inline int is_in_connection_close()
    {
        if (flags & PROXY_REQUEST)
        {
            std::string connection = this->input_headers["Proxy-Connectioin"];
            return (connection.empty() || iequals(connection, "keep-alive"));
        }
        else
        {
            std::string connection = this->input_headers["Connection"];
            return (!connection.empty() && iequals(connection, "close"));
        }
    }
    inline int is_out_connection_close()
    {
        if (flags & PROXY_REQUEST)
        {
            std::string connection = output_headers["Proxy-Connectioin"];
            return (connection.empty() || iequals(connection, "keep-alive"));
        }
        else
        {
            std::string connection = output_headers["Connection"];
            return (!connection.empty() && iequals(connection, "close"));
        }
    }
    inline int is_connection_close()
    {
        return is_in_connection_close() || is_out_connection_close();
    }

    int get_body_length();

    enum message_read_status parse_firstline(buffer *buf);
    enum message_read_status parse_headers(buffer *buf);
    enum message_read_status handle_chunked_read(buffer *buf);

    void send_error(int error, std::string reason);
    void send_not_found();
    void send_page(buffer *buf);
    void send_reply(int code, const std::string &reason, buffer *buf);
    void send_reply_start(int code, const std::string &reason);
    void send_reply_chunk(buffer *buf);
    void send_reply_end();

    void make_header();

  private:
    void __send(buffer *databuf);

    int __parse_request_line(std::string line);
    int __parse_response_line(std::string line);

    void __make_header_request();
    void __make_header_response();
};

} // namespace eve
