
<head>

<?php

	include("playlist.php"); //vecFile



	$debug = 0; //set 0 to use plugins -- 1 for guinea pigs
	
	//generate ungique id for log
		$logid = $_GET["logid"];
		if ( isset($logid) == false )
		{
			$time = mktime(); 	srand($time);
			$logid = (dechex($time).''.dechex(rand()));
		}//endif
		
			echo("<p> Log id: ".$logid."</p>");
			
			
	//	$numMovie = $_GET["numMovie"];
	//if ( isset($numMovie) == false ) { $numMovie  = 4; }
	$numMovie = 5;
	
		if ($numMovie < 1) { $numMovie = 1; }
		if ($numMovie > 16) { $numMovie = 16; }


		
	$seed = $_GET["seed"];
	if ( isset($seed) == false ) { $seed = 0; }
		srand( $seed ); //set random seed

	if ( isset($maxWait) == false )	{		$maxWait = 10000; 	}
	if ( isset($initSleep) == false )	{	$initSleep = 0; }	
		
		if ($debug >= 1) { $maxWait = 500; $initSleep = 0; }
		
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
	
//	echo('<p> Number of movies: <b>');
//	echo($numMovie . "/16 </b></p> ");
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


var left = 0;
var cur = 0;
var cycle = 0;

var debug = parseInt( "<?php echo $debug ?>" );  //1; //plugin is only for linux yet and im working in windows -- hence this variable
var numMovie = parseInt( "<?php echo $numMovie ?>" );
var initSleep  = parseInt("<?php echo $initSleep ?>");
var rtspopt = "<?php echo $rtspopt ?>";

var curBig = -1;
var	logid = "<?php echo $logid ?>";
var seed = parseInt( "<?php echo $seed ?>" );


$(document).ready(function() {

		logMessage("test7 start ");	
		logMessage("seed: "+seed);
		logMessage("number of movies: "+numMovie);
	
		var i = 0;
		for (i = 0; i < numMovie; i++)
		{
			vecPlayer[i] = document.getElementById("player"+i);
		
		}//nexti
	
	
		
	
		setTimeout("enableButton()", initSleep);		
		//setTimeout("testMovie()", initSleep);		
		setTimeout("timeUpdate()", 250);
		
		left = initSleep;
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
	
	function enableButton()
	{
			//document.getElementById("startbtn").removeAttribute("disabled");
		document.getElementById("startplace").innerHTML =
			'<p> <input id="startbtn" onclick="testMovie();" type="button" value="Start">   </p>';
		
		logMessage("Plugin ready -- Start button enabled ");
	}//ennablebtn

	function testMovie()
	{
		document.getElementById("startplace").innerHTML ="";
		//logMessage("Plugin ready ");	
			logMessage("Starting test ");
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
		
		function openMovie()
		{
			var i;
			
			cur = 0;
			
			for (i = 0; i < numMovie; i++)
			{
				if (debug == 0)
				{
					if (vecPlayer[i] == undefined) { document.write("player"+i+" is undefined"); } 
					vecPlayer[i].setOptions('rtsp_transport',rtspopt);
					vecPlayer[i].open( vecPlay[cur] );
				}//endif
				
					logMessage("player "+i+" open: " + vecPlay[cur] );	
				
				cur += 1;
				if (cur >= vecPlay.length) { cur = 0; }
			}//nexti
	
			
	
			document.getElementById("state").innerHTML = "Openmovie";
			left = 2000; //wait 2 sec before starting movie
			//left = 100;
			if (debug >= 0) { left = 0;}
			setTimeout("cycleMovie()", left);
		}//openmov
		
		
		function cycleMovie()
		{	
			showAll();
		
			setSmall(curBig);
				curBig += 1;
			
			if (curBig >= numMovie) { curBig = 0; }
				setBig(curBig);
				
			var curHidden = curBig + (numMovie-1);
				if (curHidden >= numMovie) { curHidden -= numMovie; }
					setHidden(curHidden);
					
					
			var curb = curBig;
			var i;
			
			
			//cycle the videos on the lower tables
			for (i = 1; i < 4; i++)
			{
				curb += 1;
				if (curb >= numMovie) {curb -= numMovie;}
			
					var gp = document.getElementById("dv"+curb);
					 gp.style.position = "absolute";
					 gp.style.left = (i*160);
					 gp.style.top = 300;

				if (debug == 0) { vecPlayer[curb].play(); }
			}//nexti
			
			
			
		
			document.getElementById("state").innerHTML = "Big: " + curBig + " Hidden: " + curHidden;
			
			document.getElementById("cycle").innerHTML = cycle;
			cycle += 1;
			
				logMessage("cycling -- Big: " + curBig + " Hidden: " + curHidden);
			
			left = vecWait[0];
				//left = 500; //debug
			setTimeout("cycleMovie()", left);
		}//cycle
		
		function showAll()
		{
			for (i = 0; i < numMovie; i++)
			{
				vecPlayer[i].style.visibility = 'visible'; 
				if ( debug == 1) {document.getElementById("debugpig"+i).style.visibility = 'visible'; }
			}//nexti
		}//showall
		
		function setSmall(id)
		{
			if (id < 0 || id >= numMovie) { return; }
			if (debug == 0) { vecPlayer[id].play(); }
		
			logMessage("small "+id);
			//vecPlayer[id].style.width = 160;
			//vecPlayer[id].style.height = 120; 
			
		
				  
			 var pl = document.getElementById("player"+id);
				  pl.style.width = 160;
				  pl.style.height = 120;
		
			if ( debug == 1) {

			//	document.getElementById("debugpig"+id).style.width = 160;
			//	document.getElementById("debugpig"+id).style.height = 120; 
				
				var gp = document.getElementById("debugpig"+id);
				  gp.style.width = 160;
				  gp.style.height = 120;
			}//endif
		}//setsmall
		
		
		function setBig(id)
		{
			if (id < 0 || id >= numMovie) { return; }
			if (debug == 0) { vecPlayer[id].play(); }
			
			logMessage("big "+id);
			//vecPlayer[id].style.width = 320;
			//vecPlayer[id].style.height = 240; 
			//if ( debug <= 0) { document.getElementById("big").appendChild( vecPlayer[id] ); }

	
				$('#dv' +id).css({
					'position' : 'fixed',
					'left'     : (400) + 'px',
					'top'      : (20) + 'px',
					'z-index'  : '1110'
				  });
				  
				  
			if (debug <= 0)
			{

				/*
				//how it was done in  ez_dragws.js:
				
				//on div
				  $('#' + dwid + 'Cont').css({
					'position' : 'fixed',
					'left'     : center_move_x + 'px',
					'top'      : center_move_y + 'px',
					'z-index'  : '1110'
				  });
				 vidobj = getVideo(dwid);
				  } 

				  //on the video itself
				  vidobj.style.width = w_new + 'px';
				  vidobj.style.height = h_new + 'px';
				*/
				
				  var pl = document.getElementById("player"+id);
				   pl.style.width = 320;
				   pl.style.height = 240;
				  
			}//endif
			
			if (debug == 1)
			{
				var gp = document.getElementById("debugpig"+id);
	  
				  gp.style.width = 320;
				  gp.style.height = 240;

				//document.getElementById("big").appendChild( 	document.getElementById("debugpig"+id) );
			}//endif
			
			//getIdx();
			//goFullscreen(id);
			
		}//setbig
		
		function setPos(mov, pos)
		{
			logMessage(" setting pos for big one at "+pos);
			
			if (debug <= 0)
			{
				var p = document.getElementById("bigp");
				// how to set position for player ?
			}//endif	
		}//setpos
		
		function setHidden(id)
		{
			if (id < 0 || id >= numMovie) { return; }
			if (debug == 0) { vecPlayer[id].pause(); }
			
			logMessage("hidden(paused) "+id);
			vecPlayer[id].style.visibility = 'hidden'; 
		
			//if ( debug <= 0) { document.getElementById("hide").appendChild( 	vecPlayer[id] ); }
			
			if (debug == 1)
			{
				document.getElementById("debugpig"+id).style.visibility = 'hidden';
			
				//document.getElementById("hide").appendChild( 	document.getElementById("debugpig"+id) );
			}//endif				
		}//sethidden
		
	
	
		function debugMsg(msg)
		{
			//document.getElementById("debugmsg").innerHTML = msg;
		}//message

</script>


</head>


<body>


<!--<p> Current movie:  <b id="curMovie"> NONE </b> </p>-->
<p> Cycle: <b id ="cycle"> 0 </b> </p>
<p> Time left: <b id="timeleft"> 0 </b> </p>
<p> State: <b id="state"> none </b> </p>

<!--
<p id="debugmsg"> ... </p>
<p>
Player:
</p>
-->

<p id="startplace">

</p>

 <p>
	
   <?php

	
	$k = 0;
	$i = 0;
	$fps = 20.0;// - ($numMovie-1) ;   //20 fps for single movie -- 5 fps for all 16 movies enabled
	echo('<p> FPS: <b> '.$fps.'</b></p>');

	if ($debug >= 1)
	{
		echo('<p> !! DEBUG MODE !! </p>');
	}//endif
	
	//echo('<div id="big" > <p>BIG </p></div>');
	echo ("<div>");
	
	//echo('<table border="1">');
		//echo("<tr>");
			for ($i = 0; $i < $numMovie; $i++)
			{	
				
				echo('<div id="dv'.$i.'">');
		
						echo(
							'<object id="player'.$i.'" type="application/x-vnd.FBSPlayer" width="160" height="120"> 
								<param name="fps" value="'.$fps.'" /> 
								<param name="loglevel" value="'.$loglevel.'"/>
								<param name="debugflag" value="'.$debugflag.'"/>
							</object>'
							);

							
							if ($debug == 1)
							{
							//echo('<p>vid</p>');
							
								$pigimg = "http://www.guineapigtoday.com/wp-content/uploads/2011/11/guinea-pig-tan.jpg";
								switch($i)
								{
									case 1:	$pigimg = "http://www.freewebs.com/cavylover1/Dougal.jpg";	break;
									case 2: $pigimg = "http://www.animalreliefcenter.org/wp-content/uploads/2009/10/guinea-pigs.jpg"; break;
									case 3: $pigimg = "http://www.petwebsite.com/guinea_pigs/images/guinea_pig_000003346687XSmall.jpg"; break;
									case 4: $pigimg = "http://silveropossum.homestead.com/GP/1/guinea_pig_mexican_hat.jpg"; break;
							
								}//swend
							
								echo ('<img id="debugpig'.$i.'" src="'.$pigimg.'" width="160" height="120">');
							}//endif
						
				echo("</div>");
			  
				
				
			}//nexti
			
		echo("</div>");
	//	echo("</tr>");
	//	echo ('</table>');
		
	//	echo (' <p id="hide"></p> ');
	
	?>
	
 </p>
  

  
 </body>
 