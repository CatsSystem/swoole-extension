PHP_INCLUDE = `php-config --includes`
PHP_LIBS = `php-config --libs`
PHP_LDFLAGS = `php-config --ldflags`
PHP_INCLUDE_DIR = `php-config --include-dir`
PHP_EXTENSION_DIR = `php-config --extension-dir`

SOURCES = $(wildcard *.cpp)
HEADERS = $(wildcard *.h)

http2_client.so: http2_client_extension.cpp
	c++ -DHAVE_CONFIG_H -g -o http2_client.so -O0 -fPIC -shared ${SOURCES} -std=c++11 -lphpx ${PHP_INCLUDE} -I${PHP_INCLUDE_DIR}\
	 -I${PHP_INCLUDE_DIR}/ext/swoole/include -I${PHP_INCLUDE_DIR}/ext/swoole
install: http2_client.so
	cp http2_client.so ${PHP_EXTENSION_DIR}/
clean:
	rm *.so