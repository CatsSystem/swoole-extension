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

	void runCallback(const Object& client);

//	void setStatus(int status){this->status = status;}
//	int getStatus(){return status;}
//
//	void setHeaders(zval* header){this->headers = header;}
//	zval* getHeaders(){return headers;}

	Object& getResponse(){return this->response;}

public:
	swString* 	buffer;

private:
	uint32_t 	stream_id;
	HTTP_METHOD type;
	Variant 	uri;
	zval* 		data;
	Variant 	callback;
//	int 		status = 0;
//	zval* 		headers = NULL;
	Object 		response;
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
