# swoole-extension
Swoole PHP-X Extension
基于Swoole PHP-X项目开发的扩展集合

# 依赖（Require）

* Swoole 1.9.10 +
* PHP-X Latest Version

## Http2 Client
支持Http2协议的客户端扩展，支持请求超时、Http2 Stream Push，回调入口统一。

Example：

```php

$client = new http2_client("127.0.0.1", "9501", false);
$client->connect(1, function($client, $errCode){
    // 根据错误码判断连接是否成功
    if($errCode != 0)
    {
        return;
    }
    // get请求，设置 1s 超时
    $client->get("/get", 1, function($client, $response){
        if($response->status == 200)
        {
            var_dump($response->headers);
            var_dump($response->body);
        }
	});
	// Stream Push 测试
	$stream = $client->openStream("/test");
	$stream->on("receive", function($client, $response){
		var_dump($response);
	});
});

```
