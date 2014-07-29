<?php

function multi($a, $b, $c) {
    var_dump(array_multisort($a, $b, $c));
    var_dump($a == array(1, 2, 3));
    var_dump($b == array(5, 6, 4));
    var_dump($c == array(8, 9, 7));
}

var_dump(array_multisort(array(3, 2, 1), array(4, 6, 5), array(7, 9, 8)));

echo "===\n";

$a_ = array(3, 2, 1);
$b_ = array(4, 6, 5);
$c_ = array(7, 9, 8);

multi($a_, $b_, $c_);

echo "===\n";

var_dump($a_ == array(3, 2, 1));
var_dump($b_ == array(4, 6, 5));
var_dump($c_ == array(7, 9, 8));
