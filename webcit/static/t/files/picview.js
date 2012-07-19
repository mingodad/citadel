<??("COND:BSTR", 1, "frame")>
<?=("head")>
<?%("COND:LOGGEDIN", 1, 1, 1, "", ="paging")>

<div id="banner"><h1><?_("Pictures in")> <?THISROOM:NAME></h1></div>
<div id="content" class="service">
	<table class="downloads_background">
	<tr><td>
		<script type="text/javascript" language="JavaScript">
			<?ITERATE("ROOM:FILES", ="files_section_onefile_picview")>
		</script>
	</td></tr>
	<tr><td>
		<script type="text/javascript">
			start_slideshow(1, $lastfile, 3000);
    
    			function start_slideshow(start_frame, end_frame, delay) {
        			setTimeout(switch_slides(start_frame,start_frame,end_frame, delay), delay);
    			}
                            
    			function switch_slides(frame, start_frame, end_frame, delay) {
        			return (function() {
            				Effect.Fade('slideshow' + frame);
		            		if (frame == end_frame) { frame = start_frame; } else { frame = frame + 1; }
            					setTimeout("Effect.Appear('slideshow" + frame + "');", 850);
            					setTimeout(switch_slides(frame, start_frame, end_frame, delay), delay + 850);
        			})
    			}
		</script>
	</td></tr>
	<th></th>
	</table>
<?=("trailing")>
<??("X", 1)>
