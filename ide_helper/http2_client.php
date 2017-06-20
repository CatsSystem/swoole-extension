<?php

/**
 * Error Code: Client Offline
 */
define("HTTP2_CLIENT_OFFLINE", -1);

/**
 * Error Code: Request Timeout
 */
define("HTTP2_CLIENT_TIMEOUT", -2);

/**
 * Error Code: Request Reset by Server
 */
define("HTTP2_CLIENT_RST_STREAM", -3);

/**
 * Class http2_client
 */
class http2_client
{
    /**
     * http2_client constructor.
     * @param string    $host       主机地址
     * @param int       $port       端口号
     * @param bool      $is_ssl     是否开启OPENSSL
     */
    public function __construct($host, $port, $is_ssl)
    {
    }

    /**
     * @param float     $timeout    超时时间，单位s
     * @param callable  $callback   回调函数 function($client, $errCode)
     */
    public function connect($timeout, $callback)
    {

    }

    /**
     * @param string        $path       请求路径
     * @param string|array  $data       请求数据
     * @param float         $timeout    超时时间，单位s
     * @param callable      $callback   回调函数 function($client, $response)
     * @return bool
     */
    public function post($path, $data, $timeout, $callback)
    {

    }

    /**
     * @param string        $path       请求路径
     * @param float         $timeout    超时时间，单位s
     * @param callable      $callback   回调函数 function($client, $response)
     * @return bool
     */
    public function get($path, $timeout, $callback)
    {

    }

    /**
     * @param string        $path       请求路径
     * @return false | http2_client_stream
     */
    public function openStream($path)
    {

    }

    /**
     * close client
     */
    public function close()
    {

    }
}

class http2_client_stream
{
    /**
     * @param string|array  $data       请求数据
     * @return bool
     */
    public function push($data)
    {

    }

    /**
     * @param callable      $callback   回调函数 function($client, $response)
     */
    public function onResult($callback)
    {

    }

    /**
     * close stream
     */
    public function close()
    {

    }
}

class http2_client_response
{
    /**
     * 状态码
     * @var int
     */
    public $status;

    /**
     * Header数组
     * @var array
     */
    public $headers;

    /**
     * 响应包体
     * @var string
     */
    public $body;
}