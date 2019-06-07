--TEST--
swoole_coroutine: http redirect
--SKIPIF--
<?php require __DIR__ . '/../include/skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

define('SECRET', RandStr::getBytes(rand(1024, 8192)));

$pm = new ProcessManager;
$pm->parentFunc = function ($pid) use ($pm) {
    go(function () use ($pm) {
        $data = httpGetBody("http://127.0.0.1:{$pm->getFreePort()}/");
        Assert::notEmpty(!empty($data));
        Assert::eq($data, SECRET);
        $pm->kill();
    });
};

$pm->childFunc = function () use ($pm) {
    go(function () use ($pm) {
        co::sleep(0.1);
        echo "co shutdown\n";
    });

    $http = new swoole_http_server('127.0.0.1', $pm->getFreePort());

    $http->set([
        'log_file' => '/dev/null',
        "worker_num" => 1,
    ]);

    $http->on("WorkerStart", function ($serv, $wid) use ($pm) {
        $pm->wakeup();
    });

    $http->on("request", function ($request, swoole_http_response $response) {
        $response->end(SECRET);
    });

    $http->start();

    echo "server shutdown\n";

    swoole_event_wait();
};

$pm->childFirst();
$pm->run();



?>
--EXPECT--
server shutdown
co shutdown
