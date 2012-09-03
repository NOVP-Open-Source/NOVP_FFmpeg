<?php
include("backlink.php");
?>

<html>


	<frameset cols="15%,85%">
	   <frame src="test2.php<?= $linkpar ?>" />
	   <frame src="empty.html"  name="testframe" />
	</frameset> 

	<noframes>
			Your browser does not handle frames!
<?php
include("backlink.php"); //vecFile
?>
<p><b><A HREF = <?php echo($rtsplink);?> target="_parent"> BACK </A></b></p>
	</noframes>

</html>