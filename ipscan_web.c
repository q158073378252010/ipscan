//    ipscan - an http-initiated IPv6 port scanner.
//
//    Copyright (C) 2011 Tim Chappell.
//
//    This file is part of ipscan.
//
//    ipscan is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with ipscan.  If not, see <http://www.gnu.org/licenses/>.

// ipscan_web.c version
// 0.01 - initial version
// 0.02 - improved HTML (transition to styles, general compliance)
// 0.03 - addition of ping functionality
// 0.04 - addition of indirect host support
// 0.05 - removal of empty HTML paragraph
// 0.06 - tidy up URIPATH and comparisons

#include "ipscan.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <strings.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <inttypes.h>


// Include resultsstruct in order to generate some of the html content
extern struct rslt_struc resultsstruct[];

void create_html_common_header(void)
{
		printf("%s%c%c\n","Content-Type:text/html;charset=UTF-8",13,10);
		printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n");
		printf("<HTML lang=\"en\">\n");
		printf("<HEAD>\n");
		printf("<META HTTP-EQUIV=\"Content-type\" CONTENT=\"text/html;charset=UTF-8\">\n");
		printf("<META NAME=\"AUTHOR\" CONTENT=\"Tim Chappell\">\n");
		printf("<META HTTP-EQUIV=\"CACHE-CONTROL\" CONTENT=\"NO-STORE, NO-CACHE, MUST-REVALIDATE, MAX-AGE=0\">\n");
		printf("<META HTTP-EQUIV=\"PRAGMA\" CONTENT=\"NO-CACHE\">\n");
		printf("<META NAME=\"COPYRIGHT\" CONTENT=\"Copyright (C) 2011 Tim Chappell.\">\n");

}

void create_json_header(void)
{
		printf("%s%c%c\n","Content-Type:text/html;charset=iso-8859-1",13,10);
}

void create_html_header(char * servername, uint64_t session, time_t timestamp, uint16_t numports, uint16_t *portlist, char * reconquery)
{
	uint16_t port,portindex,i;

	create_html_common_header();

	printf("<TITLE>IPv6 Universal TCP Port Scanner Version %s</TITLE>\n", IPSCAN_VER);
	printf("<SCRIPT type = \"text/javascript\" language=\"javascript\">\n");
	printf("<!--  to hide script contents from old browsers\n");
	printf("var myInterval = 0;\n");
	printf("var myBlink = 0;\n");
	printf("var fetches = 0;\n");
	printf("var request = \"\";\n");
	printf("var initreq = \"\";\n");
	printf("var updateurl = \"\";\n");
	printf("var lastupdate = 0;\n");
	printf("var starturl = \""URIPATH"/"EXENAME"?beginscan=%d&session=%"PRIu64"&starttime=%"PRIu32"&%s\";\n",\
			MAGICBEGIN, session, (uint32_t)timestamp, reconquery);
	// create a prefilled array containing the list of ports to be tested
	printf("var portlist = [");
	printf("\"indhost\", %d", (0+IPSCAN_PROTO_ICMPV6));
	for (portindex=0; portindex<numports; portindex++)
	{
		port=portlist[portindex];
		printf(" ,%d", port);
	}
	printf(" ];\n");
	// create a prefilled array containing the testing state of each port to be tested, defaults to PORTUNKNOWN
	printf("var state = [");
	printf(" \"indhost\", %d", PORTUNKNOWN);
	for (portindex=0; portindex<numports; portindex++)
	{
		port=portlist[portindex];
		printf(" ,%d", PORTUNKNOWN);
	}
	printf(" ];\n");
	// create a prefilled array containing the potential states returned for each port
	printf("var retvals = [");
	for (i=0; PORTEOL != resultsstruct[i].returnval; i++)
	{
		if (0 == i) printf(" %d",resultsstruct[i].returnval); else printf(" ,%d",resultsstruct[i].returnval);
	}
	printf(" ];\n");
	// create a prefilled array containing the text label (shorthand) describing each of the potential states returned for each port
	printf("var labels = [");
	for (i=0; PORTEOL != resultsstruct[i].returnval; i++)
	{
		if (0 == i) printf(" \"%s\"",resultsstruct[i].label); else printf(" ,\"%s\"",resultsstruct[i].label);
	}
	printf(" ];\n");
	// create a prefilled array containing the colour to be applied to each of the potential states returned for each port
	printf("var colours = [");
	for (i=0; PORTEOL != resultsstruct[i].returnval; i++)
	{
		if (0 == i) printf(" \"%s\"",resultsstruct[i].colour); else printf(" ,\"%s\"",resultsstruct[i].colour);
	}
	printf(" ];\n");
	// create an HTTP object which copes with each of the various browser vagaries
	printf("function makeHttpObject()\n");
	printf("{\n");
	printf("	try {return new XMLHttpRequest();}\n");
	printf("	catch (error) {}\n");
	printf("	try {return new ActiveXObject(\"Msxml2.XMLHTTP\");}\n");
	printf("	catch (error) {}\n");
	printf("	try {return new ActiveXObject(\"Microsoft.XMLHTTP\");}\n");
	printf("	catch (error) {}\n");
	printf("	throw new Error(\"Could not create HTTP request object.\");\n");
	printf("}\n");
	// function to "blink" the test running state during test execution
	printf("function blink()\n");
	printf("{\n");
	printf("	if (document.getElementById(\"scanstate\").style.color == \"red\")\n");
	printf("	{\n");
	printf("		document.getElementById(\"scanstate\").style.color = \"black\";\n");
	printf("	}\n");
	printf("	else\n");
	printf("	{\n");
	printf("		document.getElementById(\"scanstate\").style.color = \"red\";\n");
	printf("	}\n");
	printf("}\n");
	// startTimer() is really the initiation function. it sets the scanstate to RUNNING to give the user confidence that things are happening.
	// then the initial GET of the starturl is performed to request that the server begins the scan.
	// finally the periodic call of update() is schedule in order to retrieve and reflect the ongoing scan status.
	printf("function startTimer()\n");
	printf("{\n");
	printf("document.getElementById(\"scanstate\").innerHTML = \"RUNNING.\";\n");
	printf("document.getElementById(\"scanstate\").style.color=\"black\";\n");
	printf("myBlink = window.setInterval( \"blink()\", 1000 );\n");
	printf("initreq = makeHttpObject();\n");
	printf("initreq.open( \"GET\", starturl, true);\n");
	printf("initreq.send(null);\n");
	printf("myInterval = window.setInterval( \"update()\", %d );\n", (JSONFETCHEVERY*1000) );
	printf("} // end function startTimer\n");
	// the update() function schedules a GET from the server and then awaits its successful completion.
	// the embedded function waits for the asynchronous HTTP 200 code to be received and then evaluates the returned JSON array.
	printf("function update()\n");
	printf("{\n");
	printf("var i = 0;\n");
	printf("++fetches;\n");
	printf("updateurl = \""URIPATH"/"EXENAME"?session=%"PRIu64"&starttime=%"PRIu32"&%s&fetch=\" + fetches;\n", session,\
			(uint32_t)timestamp,reconquery);
	printf("if (fetches >%d)\n",(int)( (6 + (numports*TIMEOUTSECS) / JSONFETCHEVERY )) );
	printf("{\n");
	printf("	window.clearInterval(myInterval);\n");
	printf("	lastupdate = 1;\n");
	printf("}\n");
	printf("request = makeHttpObject();\n");
	// third param determines sync/async fetch true=async
	printf("request.open( \"GET\", updateurl, true);\n");
	printf("request.onreadystatechange = function()\n");
	printf("{\n");
	printf("	if (request.readyState == 4 && request.status == 200)\n");
	printf("	{\n");
	printf(" 		var lateststate = eval( '(' + request.responseText + ')' );\n");
	printf("		if (lateststate.length > 1)\n");
	printf("		{\n");
	// if we've received a complete set of results for the ports under test then stop the periodic tasks
	printf("			if (lateststate.length > portlist.length)\n");
	printf("			{\n");
	printf("				window.clearInterval(myInterval);\n");
	printf("				window.clearInterval(myBlink);\n");
	printf("			}\n");
	printf("			for (i = 0 ; i < (lateststate.length -1); i++)\n");
	printf("			{\n");
	printf("				state[i] = lateststate[i];\n");
	printf("			}\n");
	printf("		}\n");
	// go around all ports
	printf("		for (i = 1; i <= %d ; i++)\n", (numports+1));
	printf("		{\n");
	printf("			var textupdate = \"%s\";\n", resultsstruct[PORTUNKNOWN].label);
	printf("			var colourupdate = \"%s\";\n", resultsstruct[PORTUNKNOWN].colour);
	printf("			var findstate = state[i];\n");
	printf("			var elemid = \"pingstate\";\n");
	printf("			if (i > 1) elemid = \"port\" + portlist[i];\n");
	printf("			if (i == 1 && findstate>=%d) findstate = state[i] - %d;\n", IPSCAN_INDIRECT_RESPONSE, IPSCAN_INDIRECT_RESPONSE);
	// find a matching return value and select the appropriate label and colour code
	printf("			for (j = 0; j < retvals.length; j++)\n");
	printf("			{\n");
	printf("				if (retvals[j] == findstate)\n");
	printf("				{\n");
	printf("					textupdate = \"Port \" + portlist[i] + \" = \" + labels[j];\n");
	// ICMPv6 PING case requires the port status (or indirect host) to be returned
	printf("					if (i==1)\n");
	printf("					{\n");
	printf("						if (state[i]>=%d)\n", IPSCAN_INDIRECT_RESPONSE);
	printf("						{\n");
	printf("							textupdate = \"INDIRECT-\" + labels[j] + \" (from \" + state[0] + \")\";\n");
	printf("						}\n");
	printf("						else\n");
	printf("						{\n");
	printf("							textupdate = labels[j];\n");
	printf("						}\n");
	printf("					}\n");
	// Colour setting
	printf("					colourupdate = colours[j];\n");
	printf("				}\n");
	printf("			}\n");
	// update the text on the page ....
	printf("			document.getElementById( elemid ).innerHTML = textupdate;\n");
	printf("			document.getElementById( elemid ).style.backgroundColor=colourupdate;\n");
	printf("		}\n");
	// update the page to reflect the fact we've finished
	printf("		if (lateststate.length > portlist.length)\n");
	printf("		{\n");
	printf("			document.getElementById(\"scanstate\").innerHTML = \"COMPLETE.\";\n");
	printf("			document.getElementById(\"scanstate\").style.color=\"black\";\n");
	printf("		}\n");
	// handle failure to complete the scan in the allocated number of updates
	printf("		else if (lateststate.length <= portlist.length && lastupdate == 1)\n");
	printf("		{\n");
	printf("			window.clearInterval(myBlink);\n");
	printf("			document.getElementById(\"scanstate\").innerHTML = \"FAILED.\";\n");
	printf("			document.getElementById(\"scanstate\").style.color=\"red\";\n");
	printf("		}\n");
	printf("	}\n");
	printf("}\n");
	printf("request.send(null);\n");
	printf("} // end function update\n");
	printf("// end hiding contents from old browsers -->\n");
	printf("</SCRIPT>\n");
	printf("</HEAD>\n");
}


void create_results_key_table(char * hostname, time_t timestamp)
{
	int i;
	char tstring[64];

	printf("<P style=\"font-weight:bold\">");
	if (strftime(tstring, sizeof(tstring),"%a,%%20%d%%20%b%%20%Y%%20%T%%20%z", localtime(&timestamp)) != 0)
	{
		// Offer the opportunity for feedback and a link to the source
		printf("If you have any queries related to the results of this scan, or suggestions for improvement/additions to its' functionality");
		printf(" then please <A href=\"mailto:%s?subject=Feedback%%20on%%20IPv6%%20scanner&amp;body=host:%%20%s,%%20time:%%20%s\">email me.</A> ",\
			EMAILADDRESS, hostname, tstring );
	}
	printf("The source code for this scanner is freely available at <A href=\"https://github.com/timsgit/ipscan\">github.</A></P>\n");

	printf("<TABLE border=\"1\">\n");
	printf("<TR style=\"text-align:left\">\n");
	printf("<TD width=\"25%%\" style=\"background-color:white\">REPORTED STATE</TD><TD width=\"75%%\" style=\"background-color:white\">MEANING</TD>\n");
	printf("</TR>\n");

	for (i=0; PORTEOL != resultsstruct[i].returnval; i++)
	{
		printf("<TR style=\"text-align:left\">\n");
		printf("<TD width=\"25%%\" style=\"background-color:%s\">%s</TD><TD width=\"75%%\" style=\"background-color:white\"> %s</TD>\n",resultsstruct[i].colour,\
				resultsstruct[i].label, resultsstruct[i].description);
		printf("</TR>\n");
	}
	printf("</TABLE>\n");
	printf("<P>NOTE: Results in the ICMPv6 ECHO REQUEST test marked as INDIRECT indicate an ICMPv6 error response was received from another host (e.g. a router or firewall) rather");
	printf(" than the host under test. In this case the address of the responding host is also displayed.</P>\n");
}

void create_html_body(char * hostname, uint64_t session, time_t timestamp, uint16_t numports, uint16_t *portlist)
{
	uint16_t portindex;
	uint16_t port;
	int position = 0;
	int last = 0;

	printf("<BODY onload = \"startTimer()\">\n");

	printf("<NOSCRIPT>\n<HR>\n");
	printf("<H3 style=\"color:red\">Your browser does not support Javascript, or else it is disabled.</H3>\n");
	printf("<P>An alternative version of this IPv6 TCP port scanner which does not use Javascript is available from ");
	printf("the following <A href=\"%s/%s\">link.</A></P>\n", URIPATH, EXETXTNAME);
	printf("<P>This alternative version does not support realtime in-browser updates and will take up to ");
	printf("%d seconds to return the results.</P>\n", (numports * TIMEOUTSECS) );
	printf("<HR>\n");
	printf("</NOSCRIPT>\n");

	printf("<H3>IPv6 TCP Port Scan Results</H3>\n");
	printf("<P>Results for host : %s</P>\n\n", hostname);

	printf("<P>Scan beginning at: %s, expected to take up to %d seconds ...</P>\n", asctime(localtime(&timestamp)), numports);

	printf("<TABLE border=\"1\">\n");
	printf("<TR style=\"text-align:left\">\n");
	printf("<TD>ICMPv6 ECHO REQUEST returned : </TD><TD style=\"background-color:%s\" id=\"pingstate\">%s</TD>\n",resultsstruct[PORTUNKNOWN].colour,resultsstruct[PORTUNKNOWN].label);
	printf("</TR>\n");
	printf("</TABLE>\n");

	printf("<P>Individual TCP port scan results:</P>\n");

	// Start of table
	printf("<TABLE border=\"1\">\n");

	for (portindex= 0; portindex < numports ; portindex++)
	{
		port = portlist[portindex];
		last = (portindex == (numports-1)) ? 1 : 0 ;

		if (position ==0) printf("<TR style=\"text-align:center\">\n");;
		printf("<TD width=\"%d%%\" style=\"background-color:%s\" id=\"port%d\">Port %d = %s</TD>\n",COLUMNPCT,resultsstruct[PORTUNKNOWN].colour, \
				port, port, resultsstruct[PORTUNKNOWN].label );
		position++;
		if (position >= MAXCOLS || last == 1) { printf("</TR>\n"); position=0; };

	}

	// end of table
	printf("</TABLE>\n");
	printf("<BR>\n");
	// ongoing status
	printf("<TABLE>\n");
	printf("<TR style=\"text-align:left;font-size:120%%;font-weight:bold\">\n");
	printf("<TD>Scan is : </TD><TD id=\"scanstate\">IDLE.</TD>\n");
	printf("</TR>\n");
	printf("</TABLE>\n");

	// Create results key table
	create_results_key_table(hostname, timestamp);

}

void create_html_body_end(void)
{
	printf("</BODY>\n");
	printf("</HTML>\n");
}

void create_html_form(uint16_t numports, uint16_t *portlist)
{
	int i;
	uint16_t port,portindex;
	int position = 0;
	int last = 0;

	printf("<TITLE>IPv6 Universal TCP Port Scanner Version %s</TITLE>\n", IPSCAN_VER);
	printf("</HEAD>\n");
	printf("<BODY>\n");
	printf("<H3 style=\"color:red\">IPv6 Universal TCP Port Scanner by Tim Chappell</H3>\n");

	// Useful source http://www.w3.org/TR/1999/REC-html401-19991224/interact/forms.html#successful-controls
	printf("<P>The default list of TCP ports that will be tested are:</P>\n");

	// Start of table
	printf("<TABLE border=\"1\">\n");
	for (portindex= 0; portindex < numports ; portindex++)
	{
		port = portlist[portindex];
		last = (portindex == (numports-1)) ? 1 : 0 ;

		if (position ==0) printf("<TR style=\"text-align:center\">\n");
		printf("<TD width=\"%d%%\">Port %d</TD>\n",COLUMNPCT,port);
		position++;
		if (position >= MAXCOLS || last == 1) { printf("</TR>\n"); position=0; };
	}
	// end of table
	printf("</TABLE>\n");

	printf("<BR>\n");
	printf("<P style=\"font-weight:bold\">1. Select whether to include the default list of ports, or not:</P>\n");

	printf("<FORM action=\""URIPATH"/"EXENAME"\" accept-charset=\"ISO-8859-1\" method=\"GET\">\n");
//	printf("<P>\n");
	printf("<INPUT type=\"radio\" name=\"includeexisting\" value=\"1\" checked> Include default ports listed above in the scan<BR>\n");
	printf("<INPUT type=\"radio\" name=\"includeexisting\" value=\"-1\"> Exclude default ports, test only those specified below<BR><BR>\n");
	printf("<P style=\"font-weight:bold\">2. Enter any custom TCP ports you wish to scan (%d-%d inclusive). Duplicate or invalid ports will be removed:</P>\n", MINVALIDPORT, MAXVALIDPORT);

	for (i = 0; i < NUMUSERDEFPORTS ; i++)
	{
		printf("<INPUT type=\"text\" value=\"\" size=\"5\" maxlength=\"5\" alt=\"Custom TCP port #%d\" name=\"customport%d\"><BR>\n", i, i);
	}
	printf("<P style=\"font-weight:bold\">3. and finally click on the Begin scan button:</P>\n");

	printf("<INPUT type=\"submit\" value=\"Begin scan\">\n");
//	printf("</P>\n");
	printf("</FORM>\n");
}
