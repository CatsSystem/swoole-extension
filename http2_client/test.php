<?php
	
$client = new http2_client("127.0.0.1", "9501", false);
$client->connect(1, function($client, $errCode){
	var_dump($errCode);
	// get请求，设置 1s 超时
    $client->get("/test", 1, function($client, $response){

    	var_dump($response);
        if($response->status == HTTP2_CLIENT_OFFLINE)
        {
            //客户端连接断开
            var_dump("disconnect");
        }
        if($response->status == HTTP2_CLIENT_TIMEOUT)
        {
            //请求超时
            var_dump("timeout");
        }
        if($response->status == HTTP2_CLIENT_RST_STREAM)
        {
            //当前请求被服务器断开
            var_dump("server close request");
        }
        if($response->status == 200)
        {
            var_dump($response->headers);
            var_dump($response->body);
        }
    });
});
?>