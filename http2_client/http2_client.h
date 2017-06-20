/*******************************************************************************
 *  This file is part of http2_client.
 *
 *  http2_client is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  http2_client is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************
 * Author: Lidanyang  <simonarthur2012@gmail.com>
 * Copyright © 2017 Lidanyang <simonarthur2012@gmail.com>
 ******************************************************************************/

#ifndef HTTP2_CLIENT_HTTP2_CLIENT_H_
#define HTTP2_CLIENT_HTTP2_CLIENT_H_

#include <string>
#include <vector>
#include <map>
#include "phpx.h"
extern "C"{
	#include "swoole.h"
	#include "php_swoole.h"
	#ifdef SW_HAVE_ZLIB
	#include <zlib.h>
	extern voidpf php_zlib_alloc(voidpf opaque, uInt items, uInt size);
	extern void php_zlib_free(voidpf opaque, voidpf address);
	extern int http_response_uncompress(z_stream *stream, swString *buffer, char *body, int length);
	#endif
}
#include <nghttp2/nghttp2.h>

using namespace std;
using namespace php;

#define FOREACH_MAP(it, map) for((it)=(map)->begin(); (it)!=(map)->end(); ++(it))
#define CPP_STRL(str) (str.c_str()), (str).length()

#define HTTP2_CLIENT_OFFLINE	 -1
#define HTTP2_CLIENT_TIMEOUT  	 -2
#define HTTP2_CLIENT_RST_STREAM  -3

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

	bool isGzip(){ return this->gzip;}
	void openGzip(){ this->gzip = 1; }

	void runCallback(const Object& client);
	void runCallback(zval* client);

	Object* getResponse(){return &this->response;}

private:
	uint32_t 	stream_id;
	HTTP_METHOD type;
	Variant 	uri;
	zval* 		data;
	Variant 	callback;
	Object 		response;
	uint8_t 	gzip = 0;

public:
	swString* 	buffer;
	swTimer_node* 	timer = NULL;
#ifdef SW_HAVE_ZLIB
    z_stream gzip_stream;
    swString *gzip_buffer = NULL;
#endif
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
    map<uint32_t, Request*>* 	request_map;
    nghttp2_hd_inflater* 		inflater;
};

void http2_client_onFrame(Object& zobject, Object& socket, swClient* cli, char* buf);
void http2_client_send_request(Object& zobject, swClient* cli, Request* request);
void http2_client_push_request(swClient* cli, uint32_t stream_id, zval* post_data);
void http2_client_close_stream(swClient* cli, uint32_t stream_id);
void http2_client_send_setting(swClient *cli);

#endif /* HTTP2_CLIENT_HTTP2_CLIENT_H_ */
