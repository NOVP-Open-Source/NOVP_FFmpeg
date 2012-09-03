<head>


<?php

include("playlist.php"); //vecFile


	$debug = 0; //set 0 to use video plugin
		
	//generate ungique id for log
		$logid = $_GET["logid"];
		if ( isset($logid) == false )
		{
			$time = mktime(); 	srand($time);
			$logid = (dechex($time).''.dechex(rand()));
		}//endif

	echo("<p> Log id: ".$logid."</p>");
	

	$seed = $_GET["seed"];

	if ( isset($seed) == false ) { $seed = 0; }
	
	srand( $seed );

	//	open>play>pause>play>stop>>>
	if ( isset($maxWait) == false )
	{	
	$maxWait = 10000; //20000;  //max msec to wait (10000 is 10sec) 
	}

	if ( isset($initSleep) == false )
	{	$initSleep = 0; }
	
	$openWait = rand(1, $maxWait);
	$startWait = rand(1, $maxWait);
	$pauseWait = rand(1, $maxWait);
	$playWait = rand(1, $maxWait);
	$stopWait = rand(1, $maxWait);

	echo('<p> Seed: '.$seed.'</p>');
//	echo($seed . "  ");
/*
	echo(' Wait:   ');
	echo( $openWait . ",   " );
	echo( $startWait . ",   ");
	echo( $pauseWait . ",   ");
	echo( $playWait . ",   ");
	echo( $stopWait . "   ");
*/

?>



<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5/jquery.min.js"></script>
<script src="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/jquery-ui.min.js"></script>

<script type="text/javascript">

var player;
var vecPlay = new Array("<?php echo implode('","', $vecFile); ?>");
var cur = 0;
var cycle = 0;
var debug = parseInt( "<?php echo $debug ?>" ); //plugin is only for linux yet and im working in windows -- hence this variable
var openWait = <?php echo $openWait ?>;
var startWait =  <?php echo $startWait ?>;
var pauseWait =  <?php echo $pauseWait ?>;
var playWait =  <?php echo $playWait ?>;
var stopWait =  <?php echo $stopWait ?>;
//open>play>pause>play>stop>>>
var timeLeft = 0;
var initSleep  = parseInt("<?php echo $initSleep ?>");
var rtspopt = "<?php echo $rtspopt ?>";

var	logid = "<?php echo $logid ?>";
var seed = parseInt( "<?php echo $seed ?>" );

$(document).ready(function() {

		 player = document.getElementById("player");
			document.getElementById("state").innerHTML = "init";
		 //debugMsg("Initialising plugin... ");
		 
		 logMessage("test1 start ");
			logMessage("seed: "+seed);
		
			setTimeout("testMovie()", initSleep);  //wait 2 sec to make sure plugin is loaded
				timeLeft = initSleep;
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
	

	function testMovie()
	{
		//startWait += 2000;	
		//setTimeout("openMovie()", 0); //openWait);
		//timeLeft = openWait;
		openMovie();
	}//testmov
	
		function timeUpdate()
		{
			timeLeft -= 250;
			if (timeLeft < 0) { timeLeft = 0;}
			document.getElementById("timeleft").innerHTML = timeLeft;
				setTimeout("timeUpdate()", 250);
		}//timeud
	
		function openMovie()
		{
			//debugMsg("Opening Movie " + vecPlay[cur]);
			document.getElementById("state").innerHTML = "open";
			document.getElementById("curMovie").innerHTML = vecPlay[cur];
			
			if (debug == 0) { player.setOptions('rtsp_transport',rtspopt); player.open( vecPlay[cur] ); }
			logMessage("open: " + vecPlay[cur] );
				 
			//debugMsg("startwait " + startWait);
			//startMovie();
				setTimeout("startMovie()", startWait+2000); //wait at least 2sec for the movie to be loaded
				timeLeft = startWait+2000;
		}//openmovie
		
		function nextMovie()
		{
			cycle += 1;
			 document.getElementById("cycle").innerHTML = cycle;
			
			cur += 1;
			 if (cur >= vecPlay.length) { cur = 0;}
		}//nextmov
	
		function startMovie()
		{
			document.getElementById("state").innerHTML = "play";
			//debugMsg("Starting Movie");
			logMessage("play");
				if (debug == 0) { player.play(); } //start movie
			setTimeout("pauseMovie()", pauseWait); //1000);
			timeLeft = pauseWait;
		}//playmov
		
		function pauseMovie()
		{
			document.getElementById("state").innerHTML = "pause";
			//debugMsg("Pausing Movie");
			logMessage("pause");
			   if (debug == 0) { player.pause(); } //pause movie
			setTimeout("playMovie()", playWait);// 1000);
			timeLeft = playWait;
		}//pausemov
		
		function playMovie()
		{
			document.getElementById("state").innerHTML = "play";
			//debugMsg("Playing Movie");
			logMessage("play");
			  if (debug == 0) {	player.play(); } //continue movie
			setTimeout("stopMovie()", stopWait); //1000);
			timeLeft = stopWait;
		}//playmov
		
		function stopMovie()
		{
			document.getElementById("state").innerHTML = "stop";
			//debugMsg("Stopping Movie");
			logMessage("stop");
			  if (debug == 0) {	player.stop(); } //stop and close movie
			nextMovie(); 
			setTimeout("openMovie()", openWait); //wait a bit and then open next movie
			timeLeft = openWait;
		}//stopmov
		
	
	
		function debugMsg(msg)
		{
			//document.getElementById("debugmsg").innerHTML = msg;
		}//message

</script>


</head>


<body>

<p> Current movie:  <b id="curMovie"> NONE </b> </p>
<p> Cycle: <b id ="cycle"> 0 </b> </p>
<p> Time left: <b id="timeleft"> 0 </b> </p>
<p> State: <b id="state"> none </b> </p>
<p id="debugmsg"> ... </p>
<p>
Player:
</p>
 <p>
   <object id="player" type="application/x-vnd.FBSPlayer" width="640" height="480">
	<param name="loglevel" value="<?= $loglevel ?>"/>
	<param name="debugflag" value="<?= $debugflag ?>"/>
  </object>
 </p>
  
 </body>
 