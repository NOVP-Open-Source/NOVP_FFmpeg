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
var debug = parseInt( "<?php echo $debug ?>" ); //plugin is only for linux yet and im working in windows -- hence this variable
var loadurl = '';
var rtspopt = "<?php echo $rtspopt ?>";

var	logid = "<?php echo $logid ?>";
var seed = parseInt( "<?php echo $seed ?>" );

$(document).ready(function() {

		 player = document.getElementById("player");
			document.getElementById("state").innerHTML = "init";
		 //debugMsg("Initialising plugin... ");
		 
			logMessage("seed: "+seed);
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
			var movielength = player.getmovielength();
			var moviepos = player.getcurrentpts();
			var percent = 0;
			if(movielength>0) {
				percent = 100 * moviepos / movielength;
			}

			var st = player.getstatus();
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

		function startMovie()
		{
			document.getElementById("state").innerHTML = "play";
			//debugMsg("Starting Movie");
			logMessage("play");
			if (debug == 0) { player.play(); } //start movie
		}//playmov
		
		function pauseMovie()
		{
			document.getElementById("state").innerHTML = "pause";
			//debugMsg("Pausing Movie");
			logMessage("pause");
			if (debug == 0) { player.pause(); } //pause movie
		}//pausemov
		
		function loadMovie()
		{
			url = document.forms['furl'].elements['url'].value;
			document.getElementById("curMovie").innerHTML = url;
			loadurl = url;
			player.setOptions('rtsp_transport',rtspopt);
			if (debug==0 ) { player.open( url ); }
			document.getElementById("state").innerHTML = "play";
			//debugMsg("Playing Movie");
		}//playmov
		
		function playMovie()
		{
			url = document.forms['furl'].elements['url'].value;
			document.getElementById("curMovie").innerHTML = url;
			player.setOptions('rtsp_transport',rtspopt);
			if (debug==0 && url != loadurl) { player.open( url ); }
			loadurl = url;
			document.getElementById("state").innerHTML = "play";
			//debugMsg("Playing Movie");
			logMessage("play");
			if (debug == 0) {	player.play(); } //continue movie
		}//playmov
		
		function stopMovie()
		{
			document.getElementById("curMovie").innerHTML = 'NONE';
			document.getElementById("state").innerHTML = "stop";
			//debugMsg("Stopping Movie");
			logMessage("stop");
			if (debug == 0) {	player.stop(); } //stop and close movie
			loadurl='';
		}//stopmov
		
		function flushMovie()
		{
			document.getElementById("state").innerHTML = "flush";
			//debugMsg("Stopping Movie");
			logMessage("flush");
			if (debug == 0) {	player.flush(); } //stop and close movie
		}//stopmov
		
		function seekPosMovie(pos)
		{
			document.getElementById("state").innerHTML = "seek pos: "+pos;
			//debugMsg("Seek Pos Movie");
			logMessage("seek pos "+pos);
			if (debug == 0) { player.seekpos(player.getcurrentpts()+pos); }
		}//stopmov
		
		function seekMovie(pos)
		{
			document.getElementById("state").innerHTML = "seek: "+pos;
			//debugMsg("Seek Movie");
			logMessage("seek "+pos);
			if (debug == 0) { player.seek(pos); }
		}//stopmov
		
	
		function setVolume(vol)
		{
			player.volume(vol);
		}
	
		function debugMsg(msg)
		{
			//document.getElementById("debugmsg").innerHTML = msg;
		}//message
		
		function setCamera()
		{
			document.forms['furl'].elements['url'].value='rtsp://axis2.ica.vpn.av.hu:554/axis-media/media.amp?videocodec=h264';
		}
		
		function setMpeg4()
		{
			document.forms['furl'].elements['url'].value='rtsp://streamer.cae-engineering.hu/20120109-180703-v413-25.mp4';
		}
		
		function setHD720()
		{
			document.forms['furl'].elements['url'].value='rtsp://streamer.cae-engineering.hu/20120411-131946-v5253-310.mp4';
		}
		
		function setHD1080()
		{
			document.forms['furl'].elements['url'].value='rtsp://streamer.cae-engineering.hu/20120411-131946-v5252-317.mp4';
		}

</script>


</head>


<body>
<table border="0" cellspacing="0" cellpadding="0"><form name='furl'>
<tr><td> Current movie: <b id="curMovie"> NONE </b></td></tr>
<tr><td><input type="text" name="url" value="rtsp://axis2.ica.vpn.av.hu:554/axis-media/media.amp?videocodec=h264" size=80> </td></tr>
<tr><td>
<input type="button" name="setcam" value="Camera" onClick="setCamera();">
<input type="button" name="setmpeg" value="Mpeg4" onClick="setMpeg4();">
<input type="button" name="sethd1" value="HD 720" onClick="setHD720();">
<input type="button" name="setdh2" value="HD 1080" onClick="setHD1080();">
</td></tr>
<tr><td>
<input type="button" name="load" value="Load" onClick="loadMovie();">
<input type="button" name="play" value="Play" onClick="playMovie();">
<input type="button" name="flush" value="Flush" onClick="flushMovie();">
<input type="button" name="stop" value="Stop" onClick="stopMovie();">
</td></tr>
<tr><td>
<input type="button" name="pause" value="Pause" onClick="pauseMovie();">
<input type="button" name="forward10" value="Seek +10 sec" onClick="seekPosMovie(+10.0);">
<input type="button" name="backward10" value="Seek -10 sec" onClick="seekPosMovie(-10.0);">
<input type="button" name="seek25" value="Seek 0%" onClick="seekMovie(0.0);">
<input type="button" name="seek25" value="Seek 25%" onClick="seekMovie(25.0);">
<input type="button" name="seek25" value="Seek 50%" onClick="seekMovie(50.0);">
<input type="button" name="seek25" value="Seek 75%" onClick="seekMovie(75.0);">
</td></tr>
<tr><td>
<input type="button" name="v0" value="Mute" onClick="setVolume(0);">
<input type="button" name="v25" value="Volume 25%" onClick="setVolume(25);">
<input type="button" name="v50" value="Volume 50%" onClick="setVolume(50);">
<input type="button" name="v75" value="Volume 75%" onClick="setVolume(75);">
<input type="button" name="v100" value="Volume 100%" onClick="setVolume(100);">
</td></tr>
<tr><td> State: <b id="state"> none </b> </td></tr>
<tr><td>
<table border="1"><tr>
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
<tr><td><div id="debugmsg"> ... </td></tr>
<tr><td>
Player:
</td></tr>
<tr><td>
   <object id="player" type="application/x-vnd.FBSPlayer" width="640" height="480">
	<param name="loglevel" value="<?= $loglevel ?>"/>
	<param name="debugflag" value="<?= $debugflag ?>"/>
  </object>
</td></tr>
</form></table>
 </body>
 