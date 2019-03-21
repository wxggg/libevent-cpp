#include <http_request.hh>
#include <http_connection.hh>
#include <util_linux.hh>
#include <http_server.hh>

#include <string>
#include <iterator>
#include <sstream>
#include <vector>
#include <iostream>
#include <cassert>

namespace eve
{

/** class http_request **/
http_request::http_request(std::shared_ptr<http_connection> conn)
    : conn(conn)
{
    this->kind = RESPONSE; // defualt is RESPONSE

    input_buffer = std::make_shared<buffer>();
    output_buffer = std::make_shared<buffer>();
}

http_request::~http_request()
{
    // std::cout << __func__ << std::endl;
}

std::shared_ptr<event_base> http_request::get_base()
{
    auto c = conn.lock();
    if (c)
        return c->get_base();
    return nullptr;
}

void http_request::send_error(int error, std::string reason)
{
    std::string err_page = "<html><head>";
    err_page += "<title>" + std::to_string(error) + " " + reason + "</title>\n";
    err_page += "</head><body>\n";
    err_page += "<h1>Method Not Implemented</h1>\n";
    err_page += "<p>Invalid method in request</p>\n";
    err_page += "</body></html>\n";

    auto buf = std::make_shared<buffer>();
    buf->push_back_string(err_page);

    this->input_headers["Connection"] = "close";
    this->set_response(error, reason);

    send_page(buf);
}

void http_request::send_not_found()
{
    std::string urihtml = this->uri;
    htmlescape(urihtml);

    std::string not_found_page = "<html><head><title>404 Not Found</title></head>";
    not_found_page += "<body><h1>Not Found</h1>\n";
    not_found_page += "<p>The requested URL " + urihtml + " was not found on this server.</p>";
    not_found_page += "</body></html>\n";

    auto buf = std::make_shared<buffer>();
    buf->push_back_string(not_found_page);

    this->set_response(HTTP_NOTFOUND, "Not Found");

    send_page(buf);
}

void http_request::send_page(std::shared_ptr<buffer> buf)
{
    if (!major || !minor)
        major = minor = 1;

    if (kind != RESPONSE)
        set_response(200, "OK");

    output_headers.clear();
    output_headers["Content-Type"] = "text/html; charset=utf-8";
    output_headers["Connection"] = "close";

    __send(buf);
}

void http_request::send_reply(int code, const std::string &reason, std::shared_ptr<buffer> buf)
{
    set_response(code, reason);
    __send(buf);
}

void http_request::send_reply_start(int code, const std::string &reason)
{
    set_response(code, reason);
    if (major == 1 && minor == 1)
    {
        output_headers["Transfer-Encoding"] = "chunked";
        chunked = 1;
    }
    make_header();
    get_connection()->start_write();
}

void http_request::send_reply_chunk(std::shared_ptr<buffer> buf)
{
    std::cerr << "[R] " << __func__ << " buf-length=" << buf->get_length() << std::endl;
    if (chunked)
    {
        std::stringstream ss;
        ss << std::hex << buf->get_length();
        get_connection()->write_string(ss.str() + "\r\n");
    }
    get_connection()->write_buffer(buf);
    if (chunked)
        get_connection()->write_string("\r\n");

    get_connection()->start_write();
}

void http_request::send_reply_end()
{
    if (chunked)
    {
        get_connection()->write_string("0\r\n\r\n");
        get_connection()->start_write();
        chunked = 0;
    }
}

enum message_read_status
http_request::parse_firstline(std::shared_ptr<buffer> buf)
{
    std::string line = buf->readline();
    if (line.empty())
        return MORE_DATA_EXPECTED;

    enum message_read_status status = ALL_DATA_READ;

    switch (kind)
    {
    case REQUEST:
        if (__parse_request_line(line) == -1)
            status = DATA_CORRUPTED;
        break;
    case RESPONSE:
        if (__parse_response_line(line) == -1)
            status = DATA_CORRUPTED;
        break;
    default:
        status = DATA_CORRUPTED;
        break;
    }
    return status;
}

enum message_read_status
http_request::parse_headers(std::shared_ptr<buffer> buf)
{
    std::string line;
    // enum message_read_status status = MORE_DATA_EXPECTED;

    std::string k, v;
    while (1)
    {
        line = buf->readline();
        if (line.empty()) /* done */
            return ALL_DATA_READ;

        /* Check if this is a continuation line */
        if (line[0] == ' ' || line[0] == '\t')
        {
            if (k.empty())
                return DATA_CORRUPTED;
            ltrim(line, " \t");
            v += line;
            this->input_headers[k] = v;
            continue;
        }

        /* Processing of header lines */
        auto pos = line.find(':', 0);
        if (pos == std::string::npos)
        {
            LOG_ERROR << "parse bad header " << line;
            continue;
        }
        k = line.substr(0, pos);
        v = line.substr(pos + 1);
        k = trim(k);
        v = trim(v);
        this->input_headers[k] = v;
    }

    return ALL_DATA_READ;
}

enum message_read_status
http_request::handle_chunked_read(std::shared_ptr<buffer> buf)
{
    std::string line;
    while (buf->get_length() > 0)
    {
        if (ntoread < 0)
        {
            /* Read chunk size */
            line = buf->readline();
            long ntoread = std::stol(line, 0, 16);
            if (line.empty() || ntoread < 0)
                return DATA_CORRUPTED;
            this->ntoread = ntoread;
            if (this->ntoread == 0)
            {
                return ALL_DATA_READ;
            }
            continue;
        }

        /* don't have enough to complete a chunk; wait for more */
        if (buf->get_length() < this->ntoread)
            return MORE_DATA_EXPECTED;

        /* Completed chunk */
        this->input_buffer->push_back_buffer(buf, (size_t)this->ntoread + 2); /* here 2 is for /r/n */
        this->ntoread = -1;
        // if (!this->chunk_cb)
        // {
        //     (*this->chunk_cb)(this);
        //     this->input_buffer->reset();
        // }
    }
    return MORE_DATA_EXPECTED;
}

int http_request::get_body_length()
{
    std::string content_length = input_headers["Content-Length"];
    std::string connection = input_headers["Connection"];

    if (content_length.empty() && connection.empty())
        ntoread = -1;
    else if (content_length.empty() && connection != "Close")
    {
        /* Bad combination, we don't know when it will end */
        LOG_ERROR << "we got content length, but the server "
                  << "wants to keep the connection open:" << connection;
        return -1;
    }
    else if (content_length.empty())
    {
        ntoread = -1;
    }
    else
    {
        long ntoread = std::stol(content_length);
        if (ntoread < 0)
        {
            LOG_ERROR << "illegal content length:" << content_length;
            return -1;
        }
        this->ntoread = ntoread;
    }
    return 0;
}

void http_request::make_header()
{
    if (kind == REQUEST)
        __make_header_request();
    else
        __make_header_response();

    for (const auto &kv : output_headers)
        get_connection()->write_string(kv.first + ": " + kv.second + "\r\n");

    get_connection()->write_string("\r\n");

    if (this->output_buffer->get_length() > 0)
    {
        /*
		 * For a request, we add the POST data, for a reply, this
		 * is the regular data.
		 */
        get_connection()->write_buffer(output_buffer);
    }
}

void http_request::__send(std::shared_ptr<buffer> databuf)
{
    this->output_buffer->push_back_buffer(databuf, -1);

    /* Adds headers to the response */
    make_header();

    get_connection()->start_write();
}

/**
 * the request should be :  method uri version
 */
int http_request::__parse_request_line(std::string line)
{
    std::istringstream iss(line);
    std::vector<std::string> words = split(line, ' ');
    if (words.size() < 3)
    {
        LOG_ERROR << "receved bad request line=" << line;
        return -1;
    }

    std::string method = words[0];
    std::string uri = words[1];
    std::string version = words[2];

    if (method == "GET")
        this->type = REQ_GET;
    else if (method == "POST")
        this->type = REQ_POST;
    else if (method == "HEAD")
        this->type = REQ_HEAD;
    else
    {
        LOG_ERROR << "bad method:" << method << " on request:" << this->remote_host;
        return -1;
    }

    if (version == "HTTP/1.0")
    {
        this->major = 1;
        this->minor = 0;
    }
    else if (version == "HTTP/1.1")
    {
        this->major = 1;
        this->minor = 1;
    }
    else
    {
        LOG_ERROR << "bad version:" << version << " on request:" << this->remote_host;
        return -1;
    }
    this->uri = uri;

    /* determine if it's a proxy request */
    if (uri.size() > 0 && uri[0] != '/')
        this->flags |= PROXY_REQUEST;

    return 0;
}

int http_request::__parse_response_line(std::string line)
{
    auto found1 = line.find(' ');
    std::string protocol = line.substr(0, found1);
    auto found2 = line.find(' ', found1 + 1);
    std::string number = line.substr(found1 + 1, found2 - found1 - 1);
    std::string readable = line.substr(found2 + 1);

    if (protocol == "HTTP/1.0")
    {
        this->major = 1;
        this->minor = 0;
    }
    else if (protocol == "HTTP/1.1")
    {
        this->major = 1;
        this->minor = 1;
    }
    else
    {
        LOG_ERROR << "bad protocol:" << protocol << " on request:" << this->remote_host;
        return -1;
    }

    this->response_code = std::stoi(number);
    this->response_code_line = readable;

    return 0;
}

void http_request::__make_header_request()
{
    output_headers.erase("Proxy-Connection");

    std::string method;
    switch (type)
    {
    case REQ_GET:
        method = "GET";
        break;
    case REQ_POST:
        method = "POST";
        break;
    case REQ_HEAD:
        method = "HEAD";
        break;
    default:
        method = "";
        break;
    }

    std::string str = method + " " + uri + " HTTP/" + std::to_string(major) + "." + std::to_string(minor) + "\r\n";
    get_connection()->write_string(str);

    /* Add the content length on a post request if missing */
    if (type == REQ_POST && output_headers["Content-Length"].empty())
    {
        std::string size = std::to_string(output_buffer->get_length());
        this->output_headers["Content-Length"] = size;
    }
}

void http_request::__make_header_response()
{
    int is_keepalive = is_connection_keepalive();
    std::string s = "HTTP/" + std::to_string(major) + "." + std::to_string(minor) + " " + std::to_string(response_code) + " " + response_code_line + "\r\n";
    get_connection()->write_string(s);

    if (major == 1)
    {
        if (minor == 1 && output_headers["Date"].empty())
            output_headers["Date"] = get_date();

        /*
		 * if the protocol is 1.0; and the connection was keep-alive
		 * we need to add a keep-alive header, too.
		 */
        if (minor == 0 && is_keepalive)
            output_headers["Connection"] = "keep-alive";

        if (minor == 1 || is_keepalive)
        {
            output_headers["Connection"] = "keep-alive";
            /* 
			 * we need to add the content length if the
			 * user did not give it, this is required for
			 * persistent connections to work.
			 */
            if (output_headers["Transfer-Encoding"].empty() && output_headers["Content-Length"].empty())
                output_headers["Content-Length"] = std::to_string(output_buffer->get_length());
        }
    }

    /* Potentially add headers for unidentified content. */
    if (output_buffer->get_length() > 0 && output_headers["Content-Type"].empty())
        output_headers["Content-Type"] = "text/html; charset=utf-8";

    /* if the request asked for a close, we send a close, too */
    if (is_in_connection_close())
    {
        output_headers.erase("Connection");
        if (flags & PROXY_REQUEST)
            output_headers["Connection"] = "Close";
        output_headers.erase("Proxy-Connection");
    }
}

} // namespace eve
