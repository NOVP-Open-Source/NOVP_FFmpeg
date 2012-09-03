<?php
include("backlink.php");
?>
<p><b><A HREF = <?php echo($backlink);?> target="_parent"> BACK </A></b></p>

<?php

$maxWait = 10000; //maximum number of milliseconds to wait

$initSleep = 0; //sleep before plugin loads

$rtsp = $_GET["rtsp"];
if($rtsp == "true") $rtsp=1;
if($hd720 == "true") $hd720=1;
if($hd1080 == "true") $hd1080=1;

if($rtsp==1) {
    if($rtspopt == "http") {
//	$host="rtsp://websp-test:8000/links/";
        $host="rtsp://streamer.cae-engineering.hu:8000/";
    } else {
//	$host="rtsp://websp-test:8000/links/";
        $host="rtsp://streamer.cae-engineering.hu/";
    }
} else {
//    $host="http://websp-test:8000/links/";
    $host="http://streamer.cae-engineering.hu/";
}


$vecFile = array();
if($hd720==1) {
    array_push($vecFile, $host."20120411-131946-v5253-310.mp4");
}
if($hd1080==1) {
    array_push($vecFile, $host."20120411-131946-v5253-310.mp4");
}
//files to play

array_push($vecFile,
	$host."20120109-180703-v413-25.mp4",
	$host."20120109-180703-v414-22.mp4",
	$host."20120109-180703-v415-32.mp4",
	$host."20120109-180703-v416-29.mp4",
	$host."20120109-180703-v417-23.mp4",
	$host."20120109-180703-v418-31.mp4",
	$host."20120109-180703-v419-30.mp4"
);

/*
array_push($vecFile,
	$host."abc7cb851xd4b53a92.mp4",
	$host."ad4dc4abexa937b54d.mp4",
	$host."af41b0283x6e781229.mp4",
	$host."ae2fa09abx27d7ac4c.mp4",
	$host."a6e0513fcx4bf06038.mp4",
	$host."acd8505d6xc77890cd.mp4",
	$host."a8e5969a3x2c8f377e.mp4",
	$host."a47dd156dx9860dff3.mp4",
	$host."afc23a83dx5bd2cef8.mp4"
);
*/

/*
array_push($vecFile,
	$host."a81c15861x8be15c97.mp4",
	$host."aa96b45dfxeecd8bc7.mp4",
	$host."a22f1011exdbe8c516.mp4",
	$host."a8c187594x621fc1b4.mp4"
);
*/

?>