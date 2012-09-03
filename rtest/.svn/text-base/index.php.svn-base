<?php
$rtsp = $_GET["rtsp"];
$rtspopt = $_GET["rtspopt"];
if($rtspopt=="")
    $rtspopt="udp";
$hd720 = $_GET["hd720"];
$hd1080 = $_GET["hd1080"];
$seed = $_GET["seed"];
$loglevel = $_GET["loglevel"];
$debugflag = $_GET["debugflag"];
$numMovie = $_GET["numMovie"];
if($seed=="")
	$seed="0";
if($loglevel=="")
	$loglevel="0";
if($debugflag=="")
	$debugflag="0";
?>

<head>



<?php
/*
echo('<p><A HREF="test1.php?seed=0"> TEST 1  --  open>play>pause>play>stop>>> </A> </p>' );
echo('<p><A HREF="test2start.php?seed=0"> TEST 2  --  Test 1  with frames </A> </p>' );
*/
?>

<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.5/jquery.min.js"></script>
<script src="http://ajax.googleapis.com/ajax/libs/jqueryui/1.8/jquery-ui.min.js"></script>

<script type="text/javascript">


	function doStuff(testId)
	{
		var seed = 0;
		var num = 0;
		
		seed = 	parseInt( document.getElementById("seed").value );
		num = 	parseInt( document.getElementById("nummov").value );
		rtsp = document.getElementById("rtsp").checked;
		rtspopt = document.getElementById("rtspopt").value;
		hd720 = document.getElementById("hd720").checked;
		hd1080 = document.getElementById("hd1080").checked;
		loglevel = 	parseInt( document.getElementById("loglevel").value );
		debugflag = 	parseInt( document.getElementById("debugflag").value );
		
		//document.getElementById("debug").innerHTML = "Seed " + seed + "   Movie " + num;
		
		switch (testId)
		{
			case 0:
				window.open("test1.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 1:
				window.open("test2start.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 2:
				window.open("test3.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 3:
				window.open("test4.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 4:
				window.open("test5start.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
		
			case 5:
				window.open("test6start.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 6:
				window.open("test7.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 7:
				window.open("test8.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 8:
				window.open("test9.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 9:
				window.open("test10.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 10:
				window.open("test11.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			case 11:
				window.open("test12.php?seed="+seed+"&rtsp="+rtsp+"&rtspopt="+rtspopt+"&numMovie="+num+"&hd720="+hd720+"&hd1080="+hd1080+"&loglevel="+loglevel+"&debugflag="+debugflag,"_self");
			break;
			
			default:
			break;
		};//swend
	}//dostuff
</script>

</head>


<body>
<h2> Parameters </h2>

<!-- <p id="debug"> ...  </p> -->

<p>Seed:
	<input  id="seed" type="text" name="seed" value="<?php echo($seed) ?>" />
</p>
<p>Log level:
	<input  id="loglevel" type="text" name="loglevel" size=6 value="<?php echo($loglevel) ?>" />
Debug flag:
	<input  id="debugflag" type="text" name="debugflag" size=6 value="<?php echo($debugflag) ?>" />
</p>

<p>Rtsp:
	<input  id="rtsp" type="checkbox" name="rtsp" value="0"
	<?php
		$rtsp = $_GET["rtsp"];
		if($rtsp=="1" || $rtsp=="true")
		{
			echo("checked");
		}
	?>
	/>
	<select id="rtspopt">
		<?php
			if($rtspopt=="udp") echo('<option value="" selected>  none  </option>' ); else echo('<option value="">  none  </option>' );
			if($rtspopt=="http") echo('<option value="http" selected>  rtsp over http  </option>' ); else echo('<option value="http">  rtsp over http  </option>' );
		 ?>
	</select>
</p>
<p>HD:
	<input  id="hd720" type="checkbox" name="hd720" value="0"
	<?php
		if($hd720=="1" || $hd720=="true")
		{
			echo("checked");
		}
	?>
	/> 720p
	&nbsp;&nbsp;&nbsp;&nbsp;
	<input  id="hd1080" type="checkbox" name="hd1080" value="0"
	<?php
		if($hd1080=="1" || $hd1080=="true")
		{
			echo("checked");
		}
	?>
	/> 1080p
</p>

<p> Number of movies:
	<select id="nummov">
		<?php
			for ($i = 1; $i < 17; $i++) { if($numMovie==$i) echo('<option selected>'.$i.'</option>' ); else echo('<option>'.$i.'</option>' ); }//nexti
		 ?>
	</select>
</p>


<h2> Tests </h2>

	<p> <input onclick="doStuff(0);" type="button" value="Test  1" /> open>play>pause>play>stop>>>  </p>
	<p> <input onclick="doStuff(1);" type="button" value="Test  2" />  Test 1  with frames  </p>
	<p> <input onclick="doStuff(2);" type="button" value="Test  3" />  Multiple movies -- not in sync  </p>
	<p> <input onclick="doStuff(3);" type="button" value="Test  4" />  Multiple movies -- in sync  </p>
	<p> <input onclick="doStuff(4);" type="button" value="Test  5" />  Test 3 with frames </p>
	<p> <input onclick="doStuff(5);" type="button" value="Test  6" />  Test 4 with frames </p>
	<p> <input onclick="doStuff(6);" type="button" value="Test  7" />  5 Movies -- 1 Big 1 Hidden </p>
	<p> <input onclick="doStuff(7);" type="button" value="Test  8" />  Function test </p>
	<p> <input onclick="doStuff(8);" type="button" value="Test  9" />  Camera test </p>
	<p> <input onclick="doStuff(9);" type="button" value="Test 10" />  Function test with wait for success</p>
	<p> <input onclick="doStuff(10);" type="button" value="Test 11" />  QuickTime javascript test</p>
	<p> <input onclick="doStuff(11);" type="button" value="Test 12" />  Dual test</p>


</body>