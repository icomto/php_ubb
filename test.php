<?php

echo "------------ TESTING ------------\n";

function ubb_size($data, $data_len, $arg, $arg_len, &$param) {
	//echo "UBB SIZE MY PARAM $param FUCK\n";
	print_r($param);
	return '<span style="font-size:'.$arg.'px">'.$data.'</span>';
}

function ubb_grin($data, $data_len, $matches, $nmatches, &$param) {
	print_r($param);
	return ":GRIN:";
}

if(!function_exists('ubb_init')) die("MOD NOT LOADED\n");

$data = file_get_contents('lib/ubb.txt');

$cached = false;
$ubb = ubb_pinit("default_ubb", $cached);
if(!$cached) {
	ubb_block_add($ubb, 'inline', 'b', '', UBB_TEMPLATE, '<b>{D}</b>');
	ubb_block_add($ubb, 'inline', 'url', '', UBB_TEMPLATE, NULL, '<a href="{A}" target="_blank">{D}</a>', NULL, 1);
	ubb_block_add($ubb, 'inline', 'fuck', '', UBB_TEMPLATE, '<a href="{A}" target="_blank">{D}</a>', NULL);

	ubb_single_add($ubb, 'smiley', ':D', 0, 0, UBB_REPLACE, ':grin:');


	//ubb_block_add($ubb, 'block', 'center', '', UBB_TEMPLATE, '<div class="center">{D}</div>');
	ubb_block_add($ubb, 'block', "left", array(
		'tag_start' => '<div style="text-align:left;">',
		'tag_end' => '</div>'));
	ubb_block_add($ubb, 'block', "right", array(
		'tag_start' => '<div class="right">',
		'tag_end' => '</div>'));
	ubb_block_add($ubb, 'url', "url", array(
		'tag_start' => '<a href="',
		'tag_middle' => '" target="_blank">',
		'tag_end' => '</a>',
		'arg_fallback_to_data' => true));
	ubb_block_add($ubb, 'block', "code", array(
		'tag_start' => '<div class=\"code-header\">',
		'tag_middle' => '</div><pre class=\"code-content\">',
		'tag_end' => '</pre>',
		'default_arg' => 'Code:',
		'ignore_childs' => true));
	ubb_block_add_cb($ubb, 'inline', "size", "ubb_size");

	//ubb_block_del($ubb, "center");


	ubb_single_add($ubb, 'smiley', ":)", ":smile:");
	ubb_single_add($ubb, 'trash', "http", "XXX");
	ubb_single_add_cb($ubb, 'smiley', ":D", "ubb_grin");
	ubb_single_add($ubb, 'url', "(http|ftp)s?://[^\\s<>\\[\\]]+\\.[^\\s<>\\[\\]]+(/[^\\s<>\\[\\]]*)?", "<a href=\"\\0\" target=\"_blank\">\\0</a>", 2, 0);
	ubb_single_add($ubb, 'url', "(http|ftp)s?://fuck\\.you(/[^\\s<>\\[\\]]*)?", "<a href=\"\\0\">\\0</a>", 2, 1);

	//ubb_single_del($ubb, "http");
}

echo str_replace("<br>", "<br>", ubb_parse($ubb, $data))."\n";

//for($i = 0; $i < 1000; $i++) $fuck = ubb_parse($ubb, $data);

ubb_pfree($ubb);

?>
