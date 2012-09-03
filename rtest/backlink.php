<?php

$rtsp = $_GET["rtsp"];
$rtspopt = $_GET["rtspopt"];
if($rtspopt=="")
    $rtspopt="udp";
$seed = $_GET["seed"];
if ( isset($seed) == false ) { $seed = 0; }
$loglevel = $_GET["loglevel"];
if ( isset($loglevel) == false ) { $loglevel = 0; }
$debugflag = $_GET["debugflag"];
if ( isset($debugflag) == false ) { $debugflag = 0; }
$hd720 = $_GET["hd720"];
$hd1080 = $_GET["hd1080"];

$numMovie = $_GET["numMovie"];
if ( isset($numMovie) == false ) { $numMovie  = 4; }
if ($numMovie < 1) { $numMovie = 1; }
if ($numMovie > 16) { $numMovie = 16; }

$link = "index.php";
$linkpar = "";
if($seed <> "")
{
    $linkpar = "seed=".$seed;
}
if($rtsp <> "")
{
    if($linkpar=="")
        $linkpar = "rtsp=".$rtsp;
    else
        $linkpar = $linkpar."&rtsp=".$rtsp;
}
if($rtspopt <> "")
{
    if($linkpar=="")
        $linkpar = "rtspopt=".$rtspopt;
    else
        $linkpar = $linkpar."&rtspopt=".$rtspopt;
}
if($hd720 <> "")
{
    if($linkpar=="")
        $linkpar = "hd720=".$hd720;
    else
        $linkpar = $linkpar."&hd720=".$hd720;
}
if($hd1080 <> "")
{
    if($linkpar=="")
        $linkpar = "hd1080=".$hd1080;
    else
        $linkpar = $linkpar."&hd1080=".$hd1080;
}
if($numMovie <> "")
{
    if($linkpar=="")
        $linkpar = "numMovie=".$numMovie;
    else
        $linkpar = $linkpar."&numMovie=".$numMovie;
}
if($loglevel <> "")
{
    if($linkpar=="")
        $linkpar = "loglevel=".$loglevel;
    else
        $linkpar = $linkpar."&loglevel=".$loglevel;
}
if($debugflag <> "")
{
    if($linkpar=="")
        $linkpar = "debugflag=".$debugflag;
    else
        $linkpar = $linkpar."&debugflag=".$debugflag;
}


if($linkpar<>"")
    $linkpar = "?".$linkpar;
$backlink = $link.$linkpar;


?>

