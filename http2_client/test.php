<?php
	
$client = new http2_client("127.0.0.1", "9501", false);
$client->connect(1, function($client, $errCode){
	var_dump($errCode);
	$stream = $client->openStream("/test");
	$stream->on("receive", function($client, $response){
		var_dump($response);
	});
});
?>