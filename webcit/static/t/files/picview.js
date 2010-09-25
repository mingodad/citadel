<??("COND:BSTR", 1, "frame")>
<?=("head")>
<?=("important_msg")><?%("COND:LOGGEDIN", 1, 1, 1, "", ="paging")>
<div id="banner">
<h1>
<?_("Pictures in")><?ROOM:NAME>
</h1>
</div>
<div id="content" class="service">
<table class="downloads_background"><tr><td>
<script type="text/javascript" language="JavaScript" > nvar fadeimages=new Array()
<?ITERATE("ROOM:FILES", ="files_section_onefile_picview")>
</script>
<tr><td><script type="text/javascript" src="static/fadeshow.js">
</script>
<script type="text/javascript" >
new fadeshow(fadeimages, 500, 400, 0, 3000, 1, "R");
</script></td><th>
<?=("trailing")>
