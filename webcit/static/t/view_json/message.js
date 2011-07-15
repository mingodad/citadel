{"message": {
	"from" : {
		"name" : "<?MAIL:SUMM:FROM("J")>",
		"email": "<?MAIL:SUMM:RFCA>"
	},
	"date" : "<?MAIL:SUMM:DATEFULL>",
	"to" : {
		"email" : "<?MAIL:SUMM:TO("J")>"
	},
	"cc" : {
		"email" : "<?MAIL:SUMM:CCCC("J")>"
	},
	"subject" : "<?MAIL:SUMM:SUBJECT("J")>"
  },
	"body" : "<?MAIL:BODY("J")>",
	"attachments" : [<?ITERATE("MAIL:MIME:ATTACH:LINKS", ="view_json_message_list_attach")>],
	"submessages" : [<?ITERATE("MAIL:MIME:ATTACH:SUBMESSAGES", ="view_json_message_inline_attach")>]
}