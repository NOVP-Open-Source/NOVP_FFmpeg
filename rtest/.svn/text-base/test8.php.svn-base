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
	
	$wait1  = rand(1, $maxWait)+2000;	// open
	$wait2  = rand(1, $maxWait)+2000;	// play
	$wait3  = rand(1, $maxWait)+2000;	// pause
	$wait4  = rand(1, $maxWait)+2000;	// play
	$wait5  = rand(1, $maxWait)+2000;	// seek forward +10
	$wait6  = rand(1, $maxWait)+2000;	// seek backward -5
	$wait7  = rand(1, $maxWait)+2000;	// puase
	$wait8  = rand(1, $maxWait)+2000;	// seek forward +10
	$wait9  = rand(1, $maxWait)+2000;	// play
	$wait10 = rand(1, $maxWait)+2000;	// pause
	$wait11 = rand(1, $maxWait)+2000;	// seek forward +10
	$wait12 = rand(1, $maxWait)+2000;	// play
	$wait13 = rand(1, $maxWait)+2000;	// pause
	$wait14 = rand(1, $maxWait)+2000;	// seek backward -5
	$wait15 = rand(1, $maxWait)+2000;	// play
	$wait16 = rand(1, $maxWait)+2000;	// stop

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
var openWait = parseInt( "<?php echo $openWait ?>" );
var startWait =  parseInt("<?php echo $startWait ?>");
var pauseWait =  parseInt("<?php echo $pauseWait ?>");
var playWait =  parseInt("<?php echo $playWait ?>");
var stopWait =  parseInt("<?php echo $stopWait ?>");
//open>play>pause>play>stop>>>
var timeLeft = 0;
var initSleep  = parseInt("<?php echo $initSleep ?>");
var rtspopt = '<?php echo $rtspopt ?>';

var waits = new Array();
var waitnum = 0;

addwait(parseInt( "<?php echo $wait1 ?>"), 'Open');
addwait(parseInt( "<?php echo $wait2 ?>"), 'Play');
addwait(parseInt( "<?php echo $wait3 ?>"), 'Pause');
addwait(parseInt( "<?php echo $wait4 ?>"), 'Play');
addwait(parseInt( "<?php echo $wait5 ?>"), 'Seek forward 10 sec');
addwait(parseInt( "<?php echo $wait6 ?>"), 'Seek backward 5 sec');
addwait(parseInt( "<?php echo $wait7 ?>"), 'Seek to 25%');
addwait(parseInt( "<?php echo $wait8 ?>"), 'Seek to 50%');
addwait(parseInt( "<?php echo $wait9 ?>"), 'Play');
addwait(parseInt( "<?php echo $wait10 ?>"), 'Pause');
addwait(parseInt( "<?php echo $wait11 ?>"), 'Seek forward 10 sec');
addwait(parseInt( "<?php echo $wait12 ?>"), 'Play');
addwait(parseInt( "<?php echo $wait13 ?>"), 'Pause');
addwait(parseInt( "<?php echo $wait14 ?>"), 'Seek backward 5 sec');
addwait(parseInt( "<?php echo $wait15 ?>"), 'Play');
addwait(parseInt( "<?php echo $wait16 ?>"), 'Stop');


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

	function addwait(timeout, message)
	{
		waits[waits.length]= new Array(timeout, message);
	}

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
		if(waitnum>=waits.length)
		{
			waitnum=0;
			nextMovie();
		}
		waittime = waits[waitnum][0];
		message = waits[waitnum][1];
		document.getElementById("curMovie").innerHTML = vecPlay[cur];
		document.getElementById("state").innerHTML = (waitnum+1)+"/"+waits.length+" "+message;
		timeLeft = waittime;
		logMessage(message+": " + vecPlay[cur] );
		switch(waitnum)
		{
			case 0:		// 1
				if (debug == 0) { player.setOptions('rtsp_transport',rtspopt); player.open( vecPlay[cur] ); }
				break;
			case 1:		// 2
				if (debug == 0) { player.play(); }
				break;
			case 2:		// 3
				if (debug == 0) { player.pause(); }
				break;
			case 3:		// 4
				if (debug == 0) { player.play(); }
				break;
			case 4:		// 5
				if (debug == 0) { player.seekpos(player.getcurrentpts()+10.0); }
				break;
			case 5:		// 6
				if (debug == 0) { player.seekpos(player.getcurrentpts()-5.0); }
				break;
			case 6:		// 7
				if (debug == 0) { player.seek(25.0); }
				break;
			case 7:		// 8
				if (debug == 0) { player.seek(50.0); }
				break;
			case 8:		// 9
				if (debug == 0) { player.play(); }
				break;
			case 9:		// 10
				if (debug == 0) { player.pause(); }
				break;
			case 10:	// 11
				if (debug == 0) { player.seekpos(player.getcurrentpts()+10.0); }
				break;
			case 11:	// 12
				if (debug == 0) { player.play(); }
				break;
			case 12:	// 13
				if (debug == 0) { player.pause(); }
				break;
			case 13:	// 14
				if (debug == 0) { player.seekpos(player.getcurrentpts()-5.0); }
				break;
			case 14:	// 15
				if (debug == 0) { player.play(); }
				break;
			case 15:	// 16
				if (debug == 0) { player.stop(); }
				break;
		}
		setTimeout("testMovie()", waittime);
		waitnum++;
	}//testmov

		function setstatus(name, activ)
		{
			if(activ) {
				document.getElementById(name).style.color='black';
				document.getElementById(name).style.backgroundColor='red';
			} else {
				document.getElementById(name).style.color='gray';
				document.getElementById(name).style.backgroundColor='white';
			}
		}

		function timeUpdate()
		{
			timeLeft -= 250;
			if (timeLeft < 0) { timeLeft = 0;}
			var movielength = player.getmovielength();
			var moviepos = player.getcurrentpts();
			var percent = 0;
			if(movielength>0) {
				percent = 100 * moviepos / movielength;
			}

			var st = player.getstatus();
			document.getElementById("timeleft").innerHTML = timeLeft;
			document.getElementById("moviepos").innerHTML = Math.round(moviepos*10.0)/10.0 + "/" + Math.round(movielength*10.0)/10.0 + " (" + Math.round(percent*10.0)/10.0 +" %)" + " status id: " + st;
			document.getElementById("statusline").innerHTML = player.getstatusline();

			setstatus('st0001',st & 1);
			setstatus('st0002',st & 2);
			setstatus('st0004',st & 4);
			setstatus('st0008',st & 8);
			setstatus('st0010',st & 16);
			setstatus('st0020',st & 32);
			setstatus('st0080',st & 128);
			setstatus('st0200',st & 512);

			setTimeout("timeUpdate()", 250);
		}//timeud

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
<table border="0" cellspacing="0" cellpadding="0">
<tr><td>Current movie:  <b id="curMovie"> NONE </b> </td></tr>
<tr><td>Cycle: <b id ="cycle"> 0 </b> </td></tr>
<tr><td>Time left: <b id="timeleft"> 0 </b> </td></tr>
<tr><td>State: <b id="state"> none </b> </td></tr>
<tr><td><table border="1"><tr>
<td><span style="color:gray;" id="st0001">Started</span></td>
<td><span style="color:gray;" id="st0002">Inited</span></td>
<td><span style="color:gray;" id="st0004">Connected</span></td>
<td><span style="color:gray;" id="st0008">Opened</span></td>
<td><span style="color:gray;" id="st0010">Error</span></td>
<td><span style="color:gray;" id="st0020">Wait for free image</span></td>
<td><span style="color:gray;" id="st0080">Pause</span></td>
<td><span style="color:gray;" id="st0200">In seek</span></td>
</tr><tr>
<td colspan="8"><span id="statusline"> </span>&nbsp;</td>
</tr></table></td></tr>
<tr><td> Movie: <b id="moviepos"> </b> </td></tr>
<tr><td><div id="debugmsg"> ... </div></td></tr>
<tr><td>Player:</td></tr>
<tr><td>
   <object id="player" type="application/x-vnd.FBSPlayer" width="640" height="480">
	<param name="loglevel" value="<?= $loglevel ?>"/>
	<param name="debugflag" value="<?= $debugflag ?>"/>
  </object>
</td></tr>
</table>
 </body>
 