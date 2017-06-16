<?php
	
$client = new http2_client("127.0.0.1", "9501", false);
$client->connect(1, function() use ($client){
	var_dump(func_get_args());
	$client->get("/test", 3, function(){
		var_dump(func_get_args());
	});
});
?>