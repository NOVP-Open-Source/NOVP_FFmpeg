

<head>

<?php

include("playlist.php"); //vecFile

	//generate ungique id for log
		$logid = $_GET["logid"];
		if ( isset($logid) == false )
		{
			$time = mktime(); 	srand($time);
			$logid = (dechex($time).''.dechex(rand()));
		}//endif

	echo("<p> Log id: ".$logid."</p>");
	
	

	srand( $seed );
	
if ( isset($maxWait) == false )
{	
	$maxWait = 10000; //20000;  //max msec to wait (10000 is 10sec)
 }
	
	$pageWait = rand(1, $maxWait);
	$pageWait *= 4;
	
	
	echo(' Seed: ');
	echo($seed . "  ");

	echo(' Wait:   ');
	echo( $pageWait . " " );

?>



<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5/jquery.min.js"></script>
<script src="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/jquery-ui.min.js"></script>

<script type="text/javascript">

var	logid = "<?php echo $logid ?>";
var	rtsp = "<?php echo $rtsp ?>";
var	rtspopt = "<?php echo $rtspopt ?>";
var	hd720 = "<?php echo $hd720 ?>";
var	hd1080 = "<?php echo $hd1080 ?>";
var	numMovie = "<?php echo $numMovie ?>";

var pageWait = parseInt( "<?php echo $pageWait ?>" );
var seed = parseInt( "<?php echo $seed ?>" );
var loglevel = parseInt( "<?php echo $loglevel ?>" );
var debugflag = parseInt( "<?php echo $debugflag ?>" );
var cycle = 0;
var testpage = "test1.php?seed=" + seed + "&logid=" + logid + "&rtsp=" + rtsp + "&rtspopt=" + rtspopt + "&numMovie=" + numMovie + "&hd720=" + hd720 + "&hd1080=" + hd1080 + "&loglevel=" + loglevel + "&debugflag=" + debugflag;
var timeLeft = 0;


$(document).ready(function() {

		logMessage("test2 start");
		logMessage("seed: "+seed);
	
		setTimeout("setTestPage()", 1000);
		timeLeft = 1000;
		setTimeout("timeUpdate()", 250);
		
});//docready

	function logMessage(logmsg)
	{
		var timestamp = Math.round((new Date()).getTime() / 1000);

		$.ajax({
		  type: "POST",
		  url: "logger/logger.php",
		  data: "timestamp="+timestamp+"&cookie="+logid+"&log="+logmsg
		});
		
	}//logmsg
	
	function timeUpdate()
		{
			timeLeft -= 250;
			if (timeLeft < 0) { timeLeft = 0;}
			document.getElementById("timeleft").innerHTML = timeLeft;
				setTimeout("timeUpdate()", 250);
		}//timeud

	function setEmpty()
	{
		cycle += 1;
		document.getElementById("cycle").innerHTML =  cycle;
		
		
		logMessage("t2 openpage  empty page");
		
		window.open("empty.html","testframe");
			setTimeout("setTestPage()", 1000);
				timeLeft = 1000;
	}//setempty
	
	function setTestPage()
	{
		logMessage("t2 openpage  test1 page");
		window.open(testpage, "testframe");
			setTimeout("setEmpty()", pageWait);
				timeLeft = pageWait;
	}//settest

</script>

</head>

<body>
	<p> Cycle: <b id="cycle"> 0</b></p>
	<p> Time left: <b id="timeleft"> 0 </b> </p>
</body>