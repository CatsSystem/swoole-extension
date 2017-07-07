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

#include "phpx.h"

#include <string>
#include <iostream>

#include "http2.h"
#include "http2_client.h"
extern "C" {
#include "swoole.h"
#include "php_swoole.h"
}
using namespace php;
using namespace std;

struct TimeoutData {
    Request* request;
    Http2Client* h2cli;
};

typedef struct TimeoutData TimeoutData;

static void http2_client_onRequestTimeout(swTimer *timer, swTimer_node *tnode) {
    TimeoutData* data = (TimeoutData *) tnode->data;
    Request* request = data->request;
    Http2Client* http2client = data->h2cli;
    Object* response = request->getResponse();
    response->set("status", HTTP2_CLIENT_TIMEOUT);
    request->runCallback();
    http2client->delRequest(request->getStreamId());
}

PHPX_METHOD(http2_client, construct) {
    if (args.count() < 3) {
        error(E_ERROR, "invalid parameters.");
        return;
    }
    Variant host = args[0];
    long port = args[1].toInt();
    bool is_ssl = args[2].toBool();

    int sock_type;
    if (is_ssl) {
        sock_type = php::constant("SWOOLE_SOCK_TCP").toInt() | php::constant("SWOOLE_SSL").toInt();
    } else {
        sock_type = php::constant("SWOOLE_SOCK_TCP").toInt();
    }
    Object swoole_client = php::newObject("swoole_client", sock_type, php::constant("SWOOLE_SOCK_ASYNC"));
    Http2Client* client = new Http2Client();

    _this.set("socket", swoole_client);
    _this.oSet("client", "Http2Client", client);
    _this.set("host", host);
    _this.set("port", port);
    _this.set("connected", false);
    _this.set("ssl", is_ssl);

    Array header, cookie;
    //header.set("keepalive", 1);
    _this.set("headers", header);
    _this.set("cookies", cookie);
}

PHPX_METHOD(http2_client, onReceive) {
    Object socket = _this.get("socket");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());
    http2_client_onFrame(_this, socket, cli, args[1].toCString());
}

PHPX_METHOD(http2_client, onConnect) {
    _this.set("connected", true);
    Object socket = _this.get("socket");
    socket.exec("send", "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());
    cli->open_length_check = 1;
    cli->protocol.get_package_length = swHttp2_get_frame_length;
    cli->protocol.package_length_size = SW_HTTP2_FRAME_HEADER_SIZE;
    cli->protocol.onPackage = php_swoole_client_onPackage;
    http2_client_send_setting(cli);
    Variant callback = _this.get("connect");
    Args params;
    params.append(_this);
    params.append(socket.get("errCode"));
    php::call(callback, params);
}

PHPX_METHOD(http2_client, onError) {
    _this.set("connected", false);
    Object socket = _this.get("socket");
    Variant callback = _this.get("connect");
    Args params;
    params.append(_this);
    params.append(socket.get("errCode"));
    php::call(callback, params);
}

PHPX_METHOD(http2_client, onClose) {
    _this.set("connected", false);
    Http2Client* client = _this.oGet<Http2Client>("client", "Http2Client");
    client->disconnect(_this);

    Variant onClose = _this.get("close");
    php::call(onClose);
}

PHPX_METHOD(http2_client, connect) {
    float timeout = args[0].toFloat();
    Variant callback = args[1];

    _this.set("connect", callback);

    Array recv, connect, error, close;
    recv.append(_this);
    recv.append("onReceive");
    error.append(_this);
    error.append("onError");
    close.append(_this);
    close.append("onClose");
    connect.append(_this);
    connect.append("onConnect");

    Object socket = _this.get("socket");

    socket.exec("on", "Receive", recv);
    socket.exec("on", "Connect", connect);
    socket.exec("on", "Error", error);
    socket.exec("on", "Close", close);

    socket.exec("connect", _this.get("host"), _this.get("port"), timeout);
}

PHPX_METHOD(http2_client, isConnected) {
    Http2Client* client = _this.oGet<Http2Client>("client", "Http2Client");
    bool connected = _this.get("connected").toBool();
    if (!connected) {
        retval = false;
        return;
    }
    retval = true;
}

PHPX_METHOD(http2_client, on) {
    string name = args[0].toString();

    Variant callback = args[1];
    _this.set(name.c_str(), callback);
}

PHPX_METHOD(http2_client, post) {
    bool connected = _this.get("connected").toBool();
    Http2Client* client = _this.oGet<Http2Client>("client", "Http2Client");
    if (!connected) {
        retval = false;
        return;
    }
    Variant path = args[0];
    Variant data = args[1];
    int timeout = args[2].toInt();
    Variant callback = args[3];
    Request* new_request = new Request(client->grantStreamId(), path, data.ptr(), callback, HTTP_POST);

    TimeoutData* timeout_data = new TimeoutData();
    timeout_data->request = new_request;
    timeout_data->h2cli = client;

    php_swoole_check_timer((int) (timeout * 1000));
    new_request->timer = SwooleG.timer.add(&SwooleG.timer, (int) (timeout * 1000), 0, (void*) timeout_data,
            http2_client_onRequestTimeout);

    client->addRequest(new_request);

    Object socket = _this.get("socket");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());

    http2_client_send_request(_this, cli, new_request);
    retval = true;
    return;
}

PHPX_METHOD(http2_client, get) {
    bool connected = _this.get("connected").toBool();
    if (!connected) {
        retval = false;
        return;
    }
    Http2Client* client = _this.oGet<Http2Client>("client", "Http2Client");
    if(client->getTimeout())
    {
        retval = false;
        return;
    }
    Variant path = args[0];
    int timeout = args[1].toInt();
    Variant callback = args[2];

    Request* new_request = new Request(client->grantStreamId(), path, NULL, callback, HTTP_GET);

    TimeoutData* timeout_data = new TimeoutData();
    timeout_data->request = new_request;
    timeout_data->h2cli = client;

    php_swoole_check_timer((int) (timeout * 1000));
    new_request->timer = SwooleG.timer.add(&SwooleG.timer, (int) (timeout * 1000), 0, (void*) timeout_data,
            http2_client_onRequestTimeout);

    client->addRequest(new_request);
    Object socket = _this.get("socket");
    swClient* cli = (swClient*) swoole_get_object(socket.ptr());
    http2_client_send_request(_this, cli, new_request);
    retval = true;
    return;
}

PHPX_METHOD(http2_client, openStream) {
    bool connected = _this.get("connected").toBool();
    if (!connected) {
        retval = false;
        return;
    }
    Http2Client* client = _this.oGet<Http2Client>("client", "Http2Client");
    if(client->getTimeout())
    {
        retval = false;
        return;
    }

    string path = args[0].toString();
    uint32_t stream_id = client->grantStreamId();
    Object stream = php::newObject("http2_client_stream");
    stream.exec("init", _this, Variant((long) stream_id));
    Request* new_request = new Request(stream_id, path, NULL, stream, HTTP_STREAM);
    client->addRequest(new_request);

    Object socket = _this.get("socket");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());

    http2_client_send_request(_this, cli, new_request);

    retval = stream;
}

PHPX_METHOD(http2_client, close) {
    _this.set("connected", false);
    Object socket = _this.get("socket");
    socket.exec("close");
}

PHPX_METHOD(http2_client, closeStream) {
    long stream_id = args[0].toInt();
    Http2Client* http2 = _this.oGet<Http2Client>("client", "Http2Client");
    http2->delRequest(stream_id);
}

PHPX_METHOD(http2_client_stream, init) {
    Object client = args[0];
    _this.set("stream_id", args[1].toInt());
    _this.set("client", client);
}

PHPX_METHOD(http2_client_stream, onResult) {
    Variant callback = args[0];
    _this.set("receive", callback);
}

PHPX_METHOD(http2_client_stream, push) {
    Variant data = args[0];
    Object client = _this.get("client");
    Http2Client* http2 = client.oGet<Http2Client>("client", "Http2Client");
    if(!http2->getRequest(_this.get("stream_id").toInt()))
    {
        retval = false;
        return;
    }
    Object socket = client.get("socket");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());

    http2_client_push_request(cli, _this.get("stream_id").toInt(), data.ptr());
    retval = true;
}

PHPX_METHOD(http2_client_stream, close) {
    Object client = _this.get("client");
    Object socket = client.get("socket");
    swClient *cli = (swClient*) swoole_get_object(socket.ptr());

    http2_client_close_stream(cli, _this.get("stream_id").toInt());
    client.exec("closeStream", _this.get("stream_id").toInt());
}

PHPX_METHOD(http2_client_response, __construct) {
    Array header;
    _this.set("statusCode", 0);
    _this.set("headers", header);
    _this.set("body", "");
}

void Http2Client_dtor(zend_resource *res) {
    Http2Client *s = static_cast<Http2Client *>(res->ptr);
    delete s;
}

PHPX_EXTENSION()
{
    Extension *extension = new Extension("http2_client", "0.0.1");
    extension->onStart = [extension]() noexcept
    {
        Class *http2_client = new Class("http2_client");
        http2_client->addMethod("__construct", http2_client_construct, CONSTRUCT);
        http2_client->addMethod(PHPX_ME(http2_client, on));
        http2_client->addMethod(PHPX_ME(http2_client, connect));
        http2_client->addMethod(PHPX_ME(http2_client, isConnected));
        http2_client->addMethod(PHPX_ME(http2_client, post));
        http2_client->addMethod(PHPX_ME(http2_client, get));
        http2_client->addMethod(PHPX_ME(http2_client, openStream));
        http2_client->addMethod(PHPX_ME(http2_client, closeStream));
        http2_client->addMethod(PHPX_ME(http2_client, close));
        http2_client->addMethod(PHPX_ME(http2_client, onReceive));
        http2_client->addMethod(PHPX_ME(http2_client, onConnect));
        http2_client->addMethod(PHPX_ME(http2_client, onError));
        http2_client->addMethod(PHPX_ME(http2_client, onClose));
        extension->registerClass(http2_client);

        Class *http2_client_stream = new Class("http2_client_stream");
        http2_client_stream->addMethod(PHPX_ME(http2_client_stream, init));
        http2_client_stream->addMethod(PHPX_ME(http2_client_stream, onResult));
        http2_client_stream->addMethod(PHPX_ME(http2_client_stream, push));
        http2_client_stream->addMethod(PHPX_ME(http2_client_stream, close));
        extension->registerClass(http2_client_stream);

        Class *http2_client_response = new Class("http2_client_response");
        http2_client_response->addMethod(PHPX_ME(http2_client_response, __construct) , CONSTRUCT);
        extension->registerClass(http2_client_response);

        extension->registerResource("Http2Client", Http2Client_dtor);
        extension->registerConstant("HTTP2_CLIENT_OFFLINE", HTTP2_CLIENT_OFFLINE);
        extension->registerConstant("HTTP2_CLIENT_TIMEOUT", HTTP2_CLIENT_TIMEOUT);
        extension->registerConstant("HTTP2_CLIENT_RST_STREAM", HTTP2_CLIENT_RST_STREAM);
    };

    extension->require("swoole");
    extension->info( { "Http2 Client support", "enabled" }, { { "author", "Lancelot" },
            { "version", extension->version }, { "date", "2017-06-14" }, });

    return extension;
}

