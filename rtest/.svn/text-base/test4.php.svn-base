
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
	
	
	
	$numMovie = $_GET["numMovie"];
	
	if ( isset($numMovie) == false ) { $numMovie  = 4; }
	
		if ($numMovie < 1) { $numMovie = 1; }
		if ($numMovie > 16) { $numMovie = 16; }


	$seed = $_GET["seed"];
	if ( isset($seed) == false ) { $seed = 0; }
		srand( $seed ); //set random seed

	if ( isset($maxWait) == false )	{		$maxWait = 10000; 	}
	if ( isset($initSleep) == false )	{	$initSleep = 0; }	
		
	//generate random numbers for each movie
		$vecWait = array();

		$num = 5; //16 * 5;
		$i = 0;

		for ($i = 0; $i < $num; $i++)
		{
			$vecWait[$i] = rand(1, $maxWait)+5000;
		}//nexti


	echo('<p> Seed: <b>');
	echo($seed . " </b></p> ");
	
	echo('<p> Number of movies: <b>');
	echo($numMovie . "/16 </b></p> ");
?>



<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5/jquery.min.js"></script>
<script src="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/jquery-ui.min.js"></script>

<script type="text/javascript">

//var player;
var vecPlay = new Array("<?php echo implode('","', $vecFile); ?>");
//var cur = 0;

//var vecCur = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );
//var vecLeft = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );
var vecWait = new Array("<?php echo implode('","', $vecWait); ?>");
var vecPlayer = new Array(); 
//var vecCycle = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );

var	logid = "<?php echo $logid ?>";

var left = 0;
var cur = 0;
var cycle = 0;

var debug = parseInt( "<?php echo $debug ?>" );  //plugin is only for linux yet and im working in windows -- hence this variable
var numMovie = parseInt( "<?php echo $numMovie ?>" );
var initSleep  = parseInt("<?php echo $initSleep ?>");
var seed = parseInt( "<?php echo $seed ?>" );
var rtspopt = "<?php echo $rtspopt ?>";

$(document).ready(function() {

	logMessage("test 4  start ");
	logMessage("seed: "+seed);
	logMessage("number of movies: "+numMovie);
	
		var i = 0;
		for (i = 0; i < numMovie; i++)
		{
			vecPlayer[i] = document.getElementById("player"+i);
		}//nexti
			
		setTimeout("testMovie()", initSleep);		
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
		//setTimeout("openMovie("+id+")", 0);
		openMovie();
	}//testmov

		function timeUpdate() 
		{
			left -= 250;
			if (left < 0) { left = 0;}
			document.getElementById("timeleft").innerHTML = left;
				setTimeout("timeUpdate()", 250);
		}//timeud
		
		function openAllMovie()
		{
			var i;
			for (i = 0 ; i < numMovie; i++)
			{
				vecPlayer[i].setOptions('rtsp_transport',rtspopt);
				vecPlayer[i].open( vecPlay[ cur ] );
				logMessage("player"+i+" open: "+vecPlay[ cur ] );
			}//openall
		}//openall
		
		function playAllMovie()
		{
			var i;
			for (i = 0 ; i < numMovie; i++)
			{
				vecPlayer[i].play();
				logMessage("player"+i+" play");
			}//openall
		}//pauseall
		
		function pauseAllMovie()
		{
			var i;
			for (i = 0 ; i < numMovie; i++)
			{
				vecPlayer[i].pause();
				logMessage("player"+i+" pause");
			}//openall
		}//pauseall
		
		function stopAllMovie()
		{
			var i;
			for (i = 0 ; i < numMovie; i++)
			{
				vecPlayer[i].stop();
				logMessage("player"+i+" stop");
			}//openall
		}//stopall
		
		function nextMovie() //id)
		{
			cycle += 1;
			document.getElementById("cycle").innerHTML = cycle;
			
			cur += 1;
			if (cur >= vecPlay.length) { cur = 0;}
		}//nextmov
		
		
		function openMovie()
		{
			
			logMessage(" open ");
			if (debug == 0) { openAllMovie(); } 
			document.getElementById("state").innerHTML = "opening";
			document.getElementById("curMovie").innerHTML = vecPlay[ cur ];
			
			
			left = vecWait[1];
			setTimeout("startMovie()", left);
		}//openmovie
		
	
	
		function startMovie()
		{
			logMessage(" start ");
			if (debug == 0) { playAllMovie(); } 
			document.getElementById("state").innerHTML = "playing";
			
			left = vecWait[2];
			setTimeout("pauseMovie()", left);
		}//playmov
		
		function pauseMovie()
		{
			logMessage(" pause ");
			if (debug == 0) { pauseAllMovie(); } 
			document.getElementById("state").innerHTML = "pausing";
			
			left = vecWait[3];
			setTimeout("playMovie()", left);
	
		}//pausemov
		
		function playMovie()
		{
			logMessage(" play ");
			if (debug == 0) { playAllMovie(); } 
			document.getElementById("state").innerHTML = "playing";
			
			left = vecWait[4];
			setTimeout("stopMovie()", left);
		}//playmov
		
		function stopMovie()
		{
			logMessage(" stop ");
			if (debug == 0) { playAllMovie(); } 
			document.getElementById("state").innerHTML = "stopping";
			
			nextMovie();
			
			left = vecWait[0];
			setTimeout("openMovie()", left);
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

<!--
<p id="debugmsg"> ... </p>
<p>
Player:
</p>
-->

 <p>
	
   <?php

	
	$k = 0;
	$i = 0;
	$fps = 20.0 - ($numMovie-1) ;   //20 fps for single movie -- 5 fps for all 16 movies enabled
	echo('<p> FPS: <b> '.$fps.'</b></p>');

	echo('<table border="1">');
		
		echo("<tr>");
			for ($i = 0; $i < $numMovie; $i++)
			{	
		
				if ($k >= 4) { echo ('</tr><tr>'); $k = 0;}
				$k += 1;
			
				echo('<td>');
		
						echo(
							'<object id="player'.$i.'" type="application/x-vnd.FBSPlayer" width="160" height="120"> 
								<param name="fps" value="'.$fps.'" /> 
								<param name="loglevel" value="'.$loglevel.'"/>
								<param name="debugflag" value="'.$debugflag.'"/>
							</object>'
							);
				echo("</td>");
			  
				
				
			}//nexti
		echo("</tr>");
		
		echo ('</table>');
	
	?>
	
 </p>
  
 </body>
 