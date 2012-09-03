
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

		$num = 16 * 5;
		$i = 0;

		for ($i = 0; $i < $num; $i++)
		{
			$vecWait[$i] = rand(1, $maxWait)+10000;
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

var	logid = "<?php echo $logid ?>";
var seed =  "<?php echo $seed ?>";

var vecCur = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );
var vecLeft = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );
var vecWait = new Array("<?php echo implode('","', $vecWait); ?>");
var vecPlayer = new Array(); 
var vecCycle = new Array(0,0,0,0  ,0,0,0,0  ,0,0,0,0 ,0,0,0,0 );

//var cycle = 0;
var debug = parseInt( "<?php echo $debug ?>" ); //plugin is only for linux yet and im working in windows -- hence this variable
var numMovie = parseInt( "<?php echo $numMovie ?>" );
var initSleep  = parseInt("<?php echo $initSleep ?>");
var rtspopt = "<?php echo $rtspopt ?>";

$(document).ready(function() {

	logMessage("test3 start");
	logMessage("seed: "+seed);
	logMessage("number of movies: "+numMovie);
	
		var i = 0;
		for (i = 0; i < numMovie; i++)
		{
			//document.getElementById("mov"+i).innerHTML = (i*99);
			vecPlayer[i] = document.getElementById("player"+i);
			vecCur[i] = 0;
			vecLeft[i] = 1000;
			vecCycle[i] = 0;
			
			document.getElementById("state"+i).innerHTML = "init plugin";
			setTimeout("testMovie("+i+")", initSleep);
			//testMovie(i);
		}//nexti
			
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


	function testMovie(id)
	{
		setTimeout("openMovie("+id+")", 0);
	}//testmov

		function timeUpdate() 
		{
			var i;
			for (i = 0 ; i < numMovie; i++)
			{
				vecLeft[i] -= 250;
				if (vecLeft[i] < 0) { vecLeft[i] = 0;}
				
					document.getElementById("timeleft"+i).innerHTML = vecLeft[i];
			}//nexti

				setTimeout("timeUpdate()", 250);
		}//timeud

	
		function getWait(id, w)
		{
			if (id < 0 || id >= 16) { return 1000; }
			if (w < 0 || w >= 5) { return 1000; }
			
			return vecWait[ (id*5) + w];
		}//getwait
	
		function setLeft(id, left)
		{
			if (id < 0 || id >= vecLeft.length) { return; }
			vecLeft[id] = left;
		}//setleft
		
		
		function nextMovie(id)
		{
			if (id < 0 || id >= vecCur.length) { return; }
			vecCycle[id] += 1;
			document.getElementById("cycle"+id).innerHTML = vecCycle[id];
			
			vecCur[id] += 1;
			 if (vecCur[id] >= vecPlay.length) { vecCur[id] = 0;}	 
		}//nextmov
		
		
		function openMovie(id)
		{
			document.getElementById("curmov"+id).innerHTML = vecPlay[ vecCur[id] ];
			document.getElementById("state"+id).innerHTML = "opening";
			
				logMessage("player"+id+" open "+vecPlay[ vecCur[id] ] );
		
			if (debug == 0) { vecPlayer[id].setOptions('rtsp_transport',rtspopt); vecPlayer[id].open( vecPlay[ vecCur[id] ] ); }
			
			var left = parseInt(getWait(id, 1)) + 2000; //wait at least 2sec for the movie to be loaded
			setTimeout("startMovie("+id+")", left );
			setLeft(id, left);
			
				
		}//openmovie
		
	
	
		function startMovie(id)
		{
			logMessage("player"+id+" start ");
				if (debug == 0) { vecPlayer[id].play(); } //start movie
			
			
			document.getElementById("state"+id).innerHTML = "play";
			
			var left = getWait(id, 2);
			setTimeout("pauseMovie("+id+")", left );
			setLeft(id, left);
			
		
		}//playmov
		
		function pauseMovie(id)
		{
			logMessage("player"+id+" pause ");
			   if (debug == 0) { vecPlayer[id].pause(); } //pause movie
			
			document.getElementById("state"+id).innerHTML = "pause";
			   
			  var left = getWait(id, 3);
			setTimeout("playMovie("+id+")", left );
			setLeft(id, left); 	
		}//pausemov
		
		function playMovie(id)
		{
			logMessage("player"+id+" play ");
			  if (debug == 0) {	vecPlayer[id].play(); } //continue movie

			document.getElementById("state"+id).innerHTML = "play";
					
			 var left = getWait(id, 4);
			setTimeout("stopMovie("+id+")", left );
			setLeft(id, left);
		}//playmov
		
		function stopMovie(id)
		{
			logMessage("player"+id+" stop ");
			  if (debug == 0) {	vecPlayer[id].stop(); } //stop and close movie
			
			document.getElementById("state"+id).innerHTML = "stop";
					
			nextMovie(id);
			
			 var left = getWait(id, 0);
			 setTimeout("openMovie("+id+")", left );
			 setLeft(id, left);
		}//stopmov
		
	
	
		function debugMsg(msg)
		{
			//document.getElementById("debugmsg").innerHTML = msg;
		}//message

</script>


</head>


<body>

<!--
<p> Current movie:  <b id="curMovie"> NONE </b> </p>
<p> Cycle: <b id ="cycle"> 0 </b> </p>
<p> Time left: <b id="timeleft"> 0 </b> </p>
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
						'<p> Current movie:  <b id="curmov'.$i.'"> none </b></p>');
					echo(
						'<p> Cycle: <b id="cycle'.$i.'">0</b></p>');
					echo(
						'<p> Timeleft: <b id="timeleft'.$i.'">0</b></p>');
					echo(
						'<p> State: <b id="state'.$i.'">n/a</b></p>');
						
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
 