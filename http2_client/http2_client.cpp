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

#include "http2_client.h"
#include "http2.h"
#include "string.h"

/******** Request start ****************/

Request::Request(uint32_t stream_id, const Variant& uri, zval* data,
		const Variant& callback, HTTP_METHOD type) {
	this->stream_id = stream_id;
	this->uri = uri;
	this->data = data;
	this->callback = callback;
	this->type = type;
	this->buffer = swString_new(8192);
	this->response = php::newObject("http2_client_response");
}

Request::~Request() {
	swString_free(this->buffer);
	if (this->timer) {
		swTimer_del(&SwooleG.timer, this->timer);
		this->timer = NULL;
	}
#ifdef SW_HAVE_ZLIB
	if(this->gzip && this->gzip_buffer)
	{
		swString_free(this->gzip_buffer);
	}
#endif

}

uint32_t Request::getStreamId() {
	return stream_id;
}

Variant& Request::getUri() {
	return uri;
}

zval* Request::getData() {
	return data;
}

Variant& Request::getCallback() {
	return callback;
}

HTTP_METHOD Request::getType() {
	return type;
}

void Request::runCallback(const Object& client) {
	if (this->type == HTTP_STREAM) {
		Object http2_client_stream = this->callback;
		Variant callback = http2_client_stream.get("receive");
		Args params;
		params.append(client);
		params.append(this->response);
		php::call(callback, params);
	} else {
		Args params;
		params.append(client);
		params.append(this->response);
		php::call(this->callback, params);
	}
}

void Request::runCallback(zval* client) {
	if (this->type == HTTP_STREAM) {
		Object http2_client_stream = this->callback;
		Variant callback = http2_client_stream.get("receive");
		Args params;
		params.append(client);
		params.append(this->response);
		php::call(callback, params);
	} else {
		Args params;
		params.append(client);
		params.append(this->response);
		php::call(this->callback, params);
	}
}

/******** Request end ****************/

/******** Client start ****************/
Http2Client::Http2Client() {
	this->stream_id = 1;

	this->request_map = new map<uint32_t, Request*>();
	this->inflater = NULL;
	int ret = nghttp2_hd_inflate_new(&this->inflater);
	if (ret != 0) {
		swoole_php_error(E_WARNING,
				"nghttp2_hd_inflate_init() failed, Error: %s[%d].",
				nghttp2_strerror(ret), ret);
		return;
	}
}

Http2Client::~Http2Client() {
	map<uint32_t, Request*>::iterator request_iter;
	FOREACH_MAP(request_iter, this->request_map)
	{
		Request* request = request_iter->second;
		if (request) {
			delete request;
			request = NULL;
		}
	}
	this->request_map->clear();
	delete this->request_map;
	this->request_map = NULL;

	nghttp2_hd_inflate_del(this->inflater);
	this->inflater = NULL;
}

uint32_t Http2Client::getStreamId() {
	return this->stream_id;
}

uint32_t Http2Client::grantStreamId() {
	uint32_t id = this->stream_id;
	this->stream_id += 2;
	return id;
}

nghttp2_hd_inflater* Http2Client::getInflater() {
	return this->inflater;
}

void Http2Client::addRequest(Request* request) {
	this->request_map->insert(
			map<uint32_t, Request*>::value_type(request->getStreamId(),
					request));
}

Request* Http2Client::getRequest(uint32_t stream_id) {
	Request* request = NULL;
	map<uint32_t, Request*>::iterator request_iter;
	request_iter = this->request_map->find(stream_id);
	if (request_iter == this->request_map->end()) {
		return NULL;
	}
	request = request_iter->second;
	return request;
}

void Http2Client::delRequest(uint32_t stream_id) {
	Request* request = NULL;
	map<uint32_t, Request*>::iterator request_iter;
	request_iter = this->request_map->find(stream_id);
	if (request_iter == this->request_map->end()) {
		return;
	}
	request = request_iter->second;
	if (request) {
		delete request;
		request = NULL;
	}
	this->request_map->erase(stream_id);
}

void Http2Client::disconnect(const Object& client) {
	map<uint32_t, Request*>::iterator request_iter;
	FOREACH_MAP(request_iter, this->request_map)
	{
		Request* request = request_iter->second;
		if (!request)
			continue;

		Object* response = request->getResponse();
		response->set("status", HTTP2_CLIENT_OFFLINE);
		if (request->getType() == HTTP_STREAM) {
			request->runCallback(client);
		} else {
			request->runCallback(client);
		}
		delete request;
		request = NULL;
	}
	this->request_map->clear();
}
/******** Client end ****************/

static sw_inline void http2_add_header(nghttp2_nv *headers, string key,
		string value) {
	headers->name = (uchar*) key.c_str();
	headers->namelen = key.length();
	headers->value = (uchar*) value.c_str();
	headers->valuelen = value.length();
}

static sw_inline void http2_add_header(nghttp2_nv *headers, string key,
		char* value, int len) {
	headers->name = (uchar*) key.c_str();
	headers->namelen = key.length();
	headers->value = (uchar*) value;
	headers->valuelen = len;
}

#ifdef SW_HAVE_ZLIB
/**
 * init zlib stream
 */
static void http2_client_init_gzip_stream(Request *request)
{
	request->openGzip();
	memset(&request->gzip_stream, 0, sizeof(request->gzip_stream));
	request->gzip_buffer = swString_new(8192);
	request->gzip_stream.zalloc = php_zlib_alloc;
	request->gzip_stream.zfree = php_zlib_free;
}
#endif

static int http2_client_parse_header(Http2Client* client, Request *request,
		int flags, char *in, size_t inlen) {
	nghttp2_hd_inflater *inflater = client->getInflater();
	Object* zresponse = request->getResponse();
	if (flags & SW_HTTP2_FLAG_PRIORITY) {
		in += 5;
		inlen -= 5;
	}

	Array headers;

	ssize_t rv;
	for (;;) {
		nghttp2_nv nv;
		int inflate_flags = 0;
		size_t proclen;

		rv = nghttp2_hd_inflate_hd(inflater, &nv, &inflate_flags, (uchar *) in,
				inlen, 1);
		if (rv < 0) {
			swoole_php_error(E_WARNING, "inflate failed, Error: %s[%zd].",
					nghttp2_strerror(rv), rv);
			return -1;
		}

		proclen = (size_t) rv;

		in += proclen;
		inlen -= proclen;

		//swTraceLog(SW_TRACE_HTTP2, "Header: %s[%d]: %s[%d]", nv.name, nv.namelen, nv.value, nv.valuelen);

		if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
			if (nv.name[0] == ':') {
				if (strncasecmp((char *) nv.name + 1, "status", nv.namelen - 1)
						== 0) {
					zresponse->set("status", atoi((char *) nv.value));
					continue;
				}
			}
#ifdef SW_HAVE_ZLIB
			else if (strncasecmp((char *) nv.name, "content-encoding", nv.namelen) == 0 && strncasecmp((char *) nv.value, "gzip", nv.valuelen) == 0)
			{
				http2_client_init_gzip_stream(request);
				if (Z_OK != inflateInit2(&request->gzip_stream, MAX_WBITS + 16))
				{
					swWarn("inflateInit2() failed.");
					return SW_ERR;
				}
			}
#endif
			Variant key = Variant((char*) nv.name, nv.namelen);
			Variant value = Variant((char*) nv.value, nv.valuelen);
			headers.set(key.toCString(), value);
		}

		if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
			nghttp2_hd_inflate_end_headers(inflater);
			break;
		}

		if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && inlen == 0) {
			break;
		}
	}
	zresponse->set("headers", headers);
	rv = nghttp2_hd_inflate_change_table_size(inflater, 4096);
	if (rv != 0) {
		return rv;
	}
	return SW_OK;
}

static int http2_client_build_header(Object& zobject, Request *req,
		char *buffer, int buffer_len) {
	int ret;
	Array zheader = zobject.get("headers");
	int index = 0;
	int find_host = 0;

	nghttp2_nv nv[1024];
	if (req->getType() == HTTP_GET) {
		http2_add_header(&nv[index++], ":method", "GET");
	} else {
		http2_add_header(&nv[index++], ":method", "POST");
	}
	char* host = req->getUri().toCString();
	http2_add_header(&nv[index++], ":path", host, strlen(host));
	if (zobject.get("ssl").toBool()) {
		http2_add_header(&nv[index++], ":scheme", "https");
	} else {
		http2_add_header(&nv[index++], ":scheme", "http");
	}
	//Host
	index++;

	if (!zheader.isNull() && !zheader.empty()) {
		for (auto i = zheader.begin(); i != zheader.end(); i++) {
			Variant key = i.key();
			Variant value = i.value();

			if (key.isNull()) {
				break;
			}
			string str_key = key.toString();
			if (str_key == "Host") {
				http2_add_header(&nv[3], ":authority", value.toString());
				find_host = 1;
			} else {
				http2_add_header(&nv[index++], str_key, value.toString());
			}
		}
	}
	if (!find_host) {
		http2_add_header(&nv[3], ":authority", zobject.get("host").toString());
	}

	Array zcookie = zobject.get("cookies");

	//http cookies
	if (!zcookie.isNull() && !zcookie.empty()) {
		Variant formstr = http_build_query(zcookie);
		if (formstr.isNull()) {
			swoole_php_error(E_WARNING, "http_build_query cookie failed.");
		} else {
			http2_add_header(&nv[3], "cookie", formstr.toString());
		}
	}

	ssize_t rv;
	size_t buflen;
	size_t i;
	size_t sum = 0;
	nghttp2_hd_deflater *deflater;
	ret = nghttp2_hd_deflate_new(&deflater, 4096);
	if (ret != 0) {
		swoole_php_error(E_WARNING,
				"nghttp2_hd_deflate_init failed with error: %s\n",
				nghttp2_strerror(ret));
		return SW_ERR;
	}

	for (i = 0; i < index; ++i) {
		sum += nv[i].namelen + nv[i].valuelen;
	}

	buflen = nghttp2_hd_deflate_bound(deflater, nv, index);
	if (buflen > buffer_len) {
		swoole_php_error(E_WARNING, "header is too large.");
		return SW_ERR;
	}
	rv = nghttp2_hd_deflate_hd(deflater, (uchar *) buffer, buflen, nv, index);
	if (rv < 0) {
		swoole_php_error(E_WARNING,
				"nghttp2_hd_deflate_hd() failed with error: %s\n",
				nghttp2_strerror((int ) rv));
		return SW_ERR;
	}

	nghttp2_hd_deflate_del(deflater);
	return rv;
}

void http2_client_send_request(Object& zobject, swClient* cli,
		Request* request) {
	char buffer[8192];
	zval* post_data = request->getData();

	int n = http2_client_build_header(zobject, request,
			buffer + SW_HTTP2_FRAME_HEADER_SIZE,
			sizeof(buffer) - SW_HTTP2_FRAME_HEADER_SIZE);
	if (n <= 0) {
		swWarn("http2_client_build_header() failed.");
		return;
	}
	if (post_data == NULL && request->getType() == HTTP_POST) {
		swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_HEADERS, n,
				SW_HTTP2_FLAG_END_STREAM | SW_HTTP2_FLAG_END_HEADERS,
				request->getStreamId());
	} else {
		swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_HEADERS, n,
				SW_HTTP2_FLAG_END_HEADERS, request->getStreamId());
	}
	cli->send(cli, buffer, n + SW_HTTP2_FRAME_HEADER_SIZE, 0);

	if (post_data) {
		if (Z_TYPE_P(post_data) == IS_ARRAY) {
			zend_size_t len;
			smart_str formstr_s = { 0 };
			char *formstr = sw_http_build_query(post_data, &len,
					&formstr_s TSRMLS_CC);
			if (formstr == NULL) {
				swoole_php_error(E_WARNING,
						"http_build_query post data failed.");
				return;
			}
			memset(buffer, 0, SW_HTTP2_FRAME_HEADER_SIZE);
			swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_DATA, len,
					SW_HTTP2_FLAG_END_STREAM, request->getStreamId());
			cli->send(cli, buffer, SW_HTTP2_FRAME_HEADER_SIZE, 0);
			cli->send(cli, formstr, len, 0);
			smart_str_free(&formstr_s);
		} else {
			swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_DATA,
					Z_STRLEN_P(post_data), SW_HTTP2_FLAG_END_STREAM,
					request->getStreamId());
			cli->send(cli, buffer, SW_HTTP2_FRAME_HEADER_SIZE, 0);
			cli->send(cli, Z_STRVAL_P(post_data), Z_STRLEN_P(post_data), 0);
		}
	}
}

void http2_client_push_request(swClient* cli, uint32_t stream_id,
		zval* post_data) {
	/**
	 * send body
	 */
	char buffer[8192];
	if (post_data) {
		if (Z_TYPE_P(post_data) == IS_ARRAY) {
			zend_size_t len;
			smart_str formstr_s = { 0 };
			char *formstr = sw_http_build_query(post_data, &len,
					&formstr_s TSRMLS_CC);
			if (formstr == NULL) {
				swoole_php_error(E_WARNING,
						"http_build_query post data failed.");
				return;
			}
			memset(buffer, 0, SW_HTTP2_FRAME_HEADER_SIZE);
			swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_DATA, len, 0,
					stream_id);
			cli->send(cli, buffer, SW_HTTP2_FRAME_HEADER_SIZE, 0);
			cli->send(cli, formstr, len, 0);
			smart_str_free(&formstr_s);
		} else {
			swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_DATA,
					Z_STRLEN_P(post_data), 0, stream_id);
			cli->send(cli, buffer, SW_HTTP2_FRAME_HEADER_SIZE, 0);
			cli->send(cli, Z_STRVAL_P(post_data), Z_STRLEN_P(post_data), 0);
		}
	}
}

void http2_client_close_stream(swClient* cli, uint32_t stream_id) {
	char buffer[8192];
	swHttp2_set_frame_header(buffer, SW_HTTP2_TYPE_SETTINGS, 0,
			SW_HTTP2_FLAG_END_STREAM, stream_id);
	cli->send(cli, buffer, SW_HTTP2_FRAME_HEADER_SIZE, 0);
}

void http2_client_send_setting(swClient *cli) {
	uint16_t id = 0;
	uint32_t value = 0;

	char frame[SW_HTTP2_FRAME_HEADER_SIZE + 18];
	memset(frame, 0, sizeof(frame));
	swHttp2_set_frame_header(frame, SW_HTTP2_TYPE_SETTINGS, 18, 0, 0);

	char *p = frame + SW_HTTP2_FRAME_HEADER_SIZE;
	/**
	 * MAX_CONCURRENT_STREAMS
	 */
	id = htons(SW_HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
	memcpy(p, &id, sizeof(id));
	p += 2;
	value = htonl(SW_HTTP2_MAX_CONCURRENT_STREAMS);
	memcpy(p, &value, sizeof(value));
	p += 4;
	/**
	 * MAX_FRAME_SIZE
	 */
	id = htons(SW_HTTP2_SETTINGS_MAX_FRAME_SIZE);
	memcpy(p, &id, sizeof(id));
	p += 2;
	value = htonl(SW_HTTP2_MAX_FRAME_SIZE);
	memcpy(p, &value, sizeof(value));
	p += 4;
	/**
	 * INIT_WINDOW_SIZE
	 */
	id = htons(SW_HTTP2_SETTINGS_INIT_WINDOW_SIZE);
	memcpy(p, &id, sizeof(id));
	p += 2;
	value = htonl(65535);
	memcpy(p, &value, sizeof(value));
	p += 4;

	cli->send(cli, frame, SW_HTTP2_FRAME_HEADER_SIZE + 18, 0);
}

void http2_client_onFrame(Object& zobject, Object& socket, swClient* cli,
		char* buf) {
	Http2Client* client = zobject.oGet<Http2Client>("client", "Http2Client");

	int type = buf[3];
	int flags = buf[4];
	int stream_id = ntohl((*(int *) (buf + 5))) & 0x7fffffff;
	uint32_t length = swHttp2_get_length(buf);
	buf += SW_HTTP2_FRAME_HEADER_SIZE;
	char frame[SW_HTTP2_FRAME_HEADER_SIZE + SW_HTTP2_FRAME_PING_PAYLOAD_SIZE];

	uint16_t id;
	uint32_t value;
	if (type == SW_HTTP2_TYPE_SETTINGS) {
		if (flags & SW_HTTP2_FLAG_ACK) {
			return;
		}

		while (length > 0) {
			id = ntohs(*(uint16_t * ) (buf));
			value = ntohl(*(uint32_t * ) (buf + sizeof(uint16_t)));
			switch (id) {
			case SW_HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS:
				client->max_concurrent_streams = value;
				swTraceLog(SW_TRACE_HTTP2, "setting: max_concurrent_streams=%d.", value);
				break;
			case SW_HTTP2_SETTINGS_INIT_WINDOW_SIZE:
				client->window_size = value;
				swTraceLog(SW_TRACE_HTTP2, "setting: init_window_size=%d.", value);
				break;
			case SW_HTTP2_SETTINGS_MAX_FRAME_SIZE:
				client->max_frame_size = value;
				swTraceLog(SW_TRACE_HTTP2, "setting: max_frame_size=%d.", value);
				break;
			case SW_HTTP2_SETTINGS_MAX_HEADER_LIST_SIZE:
				client->max_header_list_size = value;
				swTraceLog(SW_TRACE_HTTP2, "setting: max_header_list_size=%d.", value);
				break;
			default:
				swWarn("unknown option[%d].", id)
				;
				break;
			}
			buf += sizeof(id) + sizeof(value);
			length -= sizeof(id) + sizeof(value);
		}

		swHttp2_set_frame_header(frame, SW_HTTP2_TYPE_SETTINGS, 0,
				SW_HTTP2_FLAG_ACK, stream_id);
		cli->send(cli, frame, SW_HTTP2_FRAME_HEADER_SIZE, 0);
		return;
	} else if (type == SW_HTTP2_TYPE_WINDOW_UPDATE) {
		client->window_size = ntohl(*(int * ) buf);
		swHttp2_set_frame_header(frame, SW_HTTP2_TYPE_WINDOW_UPDATE, 0,
				SW_HTTP2_FLAG_ACK, stream_id);
		return;
	} else if (type == SW_HTTP2_TYPE_PING) {
		swHttp2_set_frame_header(frame, SW_HTTP2_TYPE_PING,
		SW_HTTP2_FRAME_PING_PAYLOAD_SIZE, SW_HTTP2_FLAG_ACK, stream_id);
		memcpy(frame + SW_HTTP2_FRAME_HEADER_SIZE,
				buf + SW_HTTP2_FRAME_HEADER_SIZE,
				SW_HTTP2_FRAME_PING_PAYLOAD_SIZE);
		cli->send(cli, frame,
		SW_HTTP2_FRAME_HEADER_SIZE + SW_HTTP2_FRAME_PING_PAYLOAD_SIZE, 0);
		return;
	} else if (type == SW_HTTP2_TYPE_GOAWAY) {
		int last_stream_id = htonl(*(int * ) (buf));
		buf += 4;
		int error_code = htonl(*(int * ) (buf));
		swWarn("last_stream_id=%d, error_code=%d.", "GOAWAY", last_stream_id,
				error_code);
		socket.exec("close");
		return;
	}

	Request* request = client->getRequest(stream_id);
	if (request == NULL) {
		return;
	}
	if (type == SW_HTTP2_TYPE_HEADERS) {
		http2_client_parse_header(client, request, flags, buf, length);
	} else if (type == SW_HTTP2_TYPE_DATA) {
#ifdef SW_HAVE_ZLIB
		if (request->isGzip())
		{
			if (http_response_uncompress(&request->gzip_stream, request->gzip_buffer, buf, length) == SW_ERR)
			{
				return;
			}
			swString_append_ptr(request->buffer, request->gzip_buffer->str, request->gzip_buffer->length);
		}
		else
#endif
		{
			swString_append_ptr(request->buffer, buf, length);
		}
	} else if (type == SW_HTTP2_TYPE_RST_STREAM) {
		int error_code = htonl(*(int * ) (buf));
		Object* response = request->getResponse();
		response->set("status", HTTP2_CLIENT_RST_STREAM);
		request->runCallback(zobject);
		client->delRequest(stream_id);
	} else {
		swWarn("unknown frame, type=%d, stream_id=%d, length=%d.", type,
				stream_id, length);
		return;
	}

	if (request->getType() == HTTP_STREAM && type == SW_HTTP2_TYPE_DATA) {
		Object* response = request->getResponse();
		response->set("body",
				Variant(request->buffer->str, request->buffer->length));

		request->runCallback(zobject);

		swString_clear(request->buffer);
	} else if (request->getType() != HTTP_STREAM
			&& (flags & SW_HTTP2_FLAG_END_STREAM)) {
		if (request->timer) {
			swTimer_del(&SwooleG.timer, request->timer);
			request->timer = NULL;
		}
		Object* response = request->getResponse();
		response->set("body",
				Variant(request->buffer->str, request->buffer->length));
		request->runCallback(zobject);

		client->delRequest(stream_id);
	}
	return;
}

