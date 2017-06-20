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

#include "swoole.h"
#include "Connection.h"
#include "http2.h"

int swHttp2_parse_frame(swProtocol *protocol, swConnection *conn, char *data,
		uint32_t length) {
	int wait_body = 0;
	int package_length = 0;

	while (length > 0) {
		if (wait_body) {
			if (length >= package_length) {
				protocol->onPackage(conn, data, package_length);
				wait_body = 0;
				data += package_length;
				length -= package_length;
				continue;
			} else {
				break;
			}
		} else {
			package_length = protocol->get_package_length(protocol, conn, data,
					length);
			if (package_length < 0) {
				return SW_ERR;
			} else if (package_length == 0) {
				return SW_OK;
			} else {
				wait_body = 1;
			}
		}
	}
	return SW_OK;
}

int swHttp2_send_setting_frame(swProtocol *protocol, swConnection *conn) {
	char setting_frame[SW_HTTP2_FRAME_HEADER_SIZE
			+ SW_HTTP2_SETTING_OPTION_SIZE * 3];
	char *p = setting_frame;
	uint16_t id;
	uint32_t value;

	swHttp2_set_frame_header(p, SW_HTTP2_TYPE_SETTINGS,
			SW_HTTP2_SETTING_OPTION_SIZE * 3, 0, 0);
	p += SW_HTTP2_FRAME_HEADER_SIZE;

	id = htons(SW_HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
	memcpy(p, &id, sizeof(id));
	value = htonl(SW_HTTP2_MAX_CONCURRENT_STREAMS);
	memcpy(p + 2, &value, sizeof(value));
	p += SW_HTTP2_SETTING_OPTION_SIZE;

	id = htons(SW_HTTP2_SETTINGS_INIT_WINDOW_SIZE);
	memcpy(p, &id, sizeof(id));
	value = htonl(SW_HTTP2_MAX_WINDOW);
	memcpy(p + 2, &value, sizeof(value));
	p += SW_HTTP2_SETTING_OPTION_SIZE;

	id = htons(SW_HTTP2_SETTINGS_MAX_FRAME_SIZE);
	memcpy(p, &id, sizeof(id));
	value = htonl(SW_HTTP2_MAX_FRAME_SIZE);
	memcpy(p + 2, &value, sizeof(value));

	return swConnection_send(conn, setting_frame, sizeof(setting_frame), 0);
}

/**
 +-----------------------------------------------+
 |                 Length (24)                   |
 +---------------+---------------+---------------+
 |   Type (8)    |   Flags (8)   |
 +-+-------------+---------------+-------------------------------+
 |R|                 Stream Identifier (31)                      |
 +=+=============================================================+
 |                   Frame Payload (0...)                      ...
 +---------------------------------------------------------------+
 */
int swHttp2_get_frame_length(swProtocol *protocol, swConnection *conn,
		char *buf, uint32_t length) {
	if (length < SW_HTTP2_FRAME_HEADER_SIZE) {
		return 0;
	}
	return swHttp2_get_length(buf) + SW_HTTP2_FRAME_HEADER_SIZE;
}
