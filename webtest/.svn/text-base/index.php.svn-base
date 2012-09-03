<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>STREAMPLAYER test page</title>
<link href="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/themes/base/jquery-ui.css" rel="stylesheet" type="text/css"/>
<link href="css/jquery-ui-1.8.17.custom.css" rel="stylesheet" type="text/css"/>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5/jquery.min.js"></script>
<script src="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/jquery-ui.min.js"></script>

<style type="text/css">
body {
	background-color: #191919;
	background-image:url(gfx/pagebg.jpg);
	background-position:center top;
	background-attachment:fixed;
	text-align:center;
	margin:0;
	padding:0;
	font-family:Arial, Helvetica, sans-serif;
	color:#fff;
}
div#playerPlugin {
	margin:0;
	margin-top:6em;
	padding:0;
	background:#000;
	text-align:center;
	box-shadow: 0px 0px 30px #000;
	position:relative;
	z-index:10;
}
div#controls {
	display:inline-block;
	position:relative;
	z-index:0;
	margin:auto;
	border-radius:0 0 1em 1em;
	margin-top:0;
	padding:1em;
	background:#333;
	background-image:url(gfx/pagebg_dark.jpg);
	background-position:center middle;
	box-shadow: 0px 0px 30px #000;
}

div#controls img {
	max-height:20px;
	max-width:20px;
	padding:5px;
	cursor:pointer;
}

div#movie_slider {
	margin-top:0.5em; !important
}
</style>
	<script type="text/javascript">

$(document).ready(function() {
	$("#controls").mouseenter(function(e) {
		downCtrl();
	});
	$("#controls").mouseleave(function(e) {
		upCtrl();
	});
	var player = document.getElementById("player");
	$("#open").click(function(e) {
		//player.addItem('http://axis1.ica.vpn.av.hu/mjpg/video.mjpg');
		player.addItem('http://streamer.cae-engineering.hu/20120109-180703-v413-25.mp4');
	});
	$("#play").click(function(e) {
		player.play();
	});
	$("#stop").click(function(e) {
		player.stop();
	});
	$("#pause").click(function(e) {
		player.pause();
	});

	var button_ids   = new Array('stop', 'open', 'play', 'pause', 'slower', 'faster', 'back', 'fwd', 'fullscreen', 'mute');	
	var button_files = new Array('stop', 'open', 'f', 'pause', 'bb', 'ff', 'bbb', 'fff', 'fullscreen', 'mute');	
	var code = '';
	for( i = 0; i < button_ids.length; i++ )
	{
		code += '$("#'+button_ids[i]+'").mouseenter(function(){$("#'+button_ids[i]+'").attr("src","gfx/'+button_files[i]+'_h.png");});$("#'+button_ids[i]+'").mouseleave(function(){$("#'+button_ids[i]+'").attr("src","gfx/'+button_files[i]+'.png");});';
	}

	$("#movie_slider").slider();
	$("#movie_slider").width("500px");
	eval(code);
	downCtrl();
});


function upCtrl(){
	$("#controls").animate({marginTop:-$("#controls").innerHeight()+45}, 400, function(){});
}

function downCtrl(){
	$("#controls").animate({marginTop:"0"}, 100, function(){});
}

</script>

</head>

<body>
<div id="playerPlugin">
  <object id="player" type="application/x-vnd.FBSPlayer" width="640" height="480">
  </object>
</div>
<div id="controls"> 
	<span id="open_close">
		<img src="gfx/stop.png"       id="stop"         alt="STOP" />
		<img src="gfx/open.png"       id="open"         alt="OPEN" />
	</span>
	<span id="play_pause">
		<img src="gfx/f.png"          id="play"         alt="PLAY" />
		<img src="gfx/pause.png"      id="pause"        alt="PAUSE" />
	</span>
	<span id="play_controls">
		<img src="gfx/bb.png"         id="slower"       alt="SLOWER" />
		<img src="gfx/ff.png"         id="faster"       alt="FASTER" />
		<img src="gfx/bbb.png"        id="back"         alt="JUMP BACK" />
		<img src="gfx/fff.png"        id="fwd"          alt="JUMP FORWARD" />
	</span>
	<span id="etc">
		<img src="gfx/fullscreen.png" id="fullscreen"   alt="FULLSCREEN" />
		<img src="gfx/mute.png"       id="mute"         alt="MUTE" />
	</span>
	<div id="movie_slider"></div>
</div>
</body>
</html>
