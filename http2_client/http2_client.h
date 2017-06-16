/*
 * http2_client.h
 *
 *  Created on: 2017年6月15日
 *      Author: lidanyang
 */

#ifndef HTTP2_CLIENT_HTTP2_CLIENT_H_
#define HTTP2_CLIENT_HTTP2_CLIENT_H_

#include <string>
#include <vector>
#include <map>
#include "phpx.h"
extern "C"{
	#include "swoole.h"
	#include "php_swoole.h"
}
#include <nghttp2/nghttp2.h>

using namespace std;
using namespace php;

#define FOREACH_MAP(it, map) for((it)=(map)->begin(); (it)!=(map)->end(); ++(it))
#define CPP_STRL(str) (str.c_str()), (str).length()

enum HTTP_METHOD
{
	HTTP_GET 	= 1,
	HTTP_POST 	= 2,
	HTTP_STREAM = 3
};

class Request
{
public:
	Request(uint32_t stream_id, const Variant& uri, zval* data, const Variant& callback, HTTP_METHOD type);
	~Request();

	uint32_t getStreamId();
	Variant& getUri();
	zval* getData();
	Variant& getCallback();
	HTTP_METHOD getType();
	Object& getResponse();

	void runCallback(const Object& client, const Object& response);

	void setStatus(int status){this->status = status;}
	int getStatus(){return status;}

	void setHeaders(zval* header){this->headers = header;}
	zval* getHeaders(){return headers;}

public:
	swString* 	buffer;

private:
	uint32_t 	stream_id;
	HTTP_METHOD type;
	Variant 	uri;
	zval* 		data;
	Variant 	callback;
	int 		status = 0;
	zval* 		headers = NULL;
};

class Http2Client
{
public:
	Http2Client();
	~Http2Client();

	uint32_t getStreamId();
	uint32_t grantStreamId();
	nghttp2_hd_inflater* getInflater();

	void addRequest(Request*);

	Request* getRequest(uint32_t stream_id);

	void delRequest(uint32_t stream_id);

	void disconnect(const Object& client);

public:
    uint32_t window_size = 0;
    uint32_t max_concurrent_streams = 0;
    uint32_t max_frame_size = 0;
    uint32_t max_header_list_size = 0;

private:
	uint32_t 					stream_id;
    nghttp2_hd_inflater* 		inflater;
    map<uint32_t, Request*>* 	request_map;
};

void http2_client_onFrame(Object& zobject, Object& socket, swClient* cli, char* buf);
void http2_client_send_request(Object& zobject, swClient* cli, Request* request);
void http2_client_push_request(swClient* cli, uint32_t stream_id, zval* post_data);
void http2_client_close_stream(swClient* cli, uint32_t stream_id);
void http2_client_send_setting(swClient *cli);

#endif /* HTTP2_CLIENT_HTTP2_CLIENT_H_ */
