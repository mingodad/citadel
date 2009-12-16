/*
 * JavaScript code to create "bubble tooltips"
 * 
 * Copyright (C) 2006 Alessandro Fulciniti [http://web-graphics.com]
 * Copyright (C) 2006 Art Cancro [http://www.citadel.org]
 *
 * The original version of this module was released into the public
 * domain.  This version is distributed as part of the Citadel system
 * under the terms of the GNU General Public License v3.
 *
 */

function btt_enableTooltips(id)
{
	var links, i, h;
	if (!document.getElementById || !document.getElementsByTagName) {
		return;
	}
	btt_AddCss();
	h = document.createElement("span");
	h.id = "btc";
	h.setAttribute("id", "btc");
	h.style.position = "absolute";
	document.getElementsByTagName("body")[0].appendChild(h);
	if (id == null) {
		links = document.getElementsByTagName("a");
	}
	else {
		links = document.getElementById(id).getElementsByTagName("a");
	}
	for (i = 0; i < links.length; i++) {
		btt_Prepare(links[i]);
	}
}

function btt_Prepare(el)
{
	var tooltip, b, s, l, ih;
	ih = el.getAttribute("btt_tooltext");
	if (!ih) {
		return;
	}
	el.removeAttribute("btt_tooltext");
	el.removeAttribute("title");
	tooltip = btt_CreateEl("span", "tooltip");
	s = btt_CreateEl("span", "top");
	s.appendChild(document.createTextNode(""));
	s.innerHTML = ih;
	tooltip.appendChild(s);
	b = btt_CreateEl("b", "bottom");
	tooltip.appendChild(b);
	btt_setOpacity(tooltip);
	el.tooltip = tooltip;
	el.onmouseover = btt_showTooltip;
	el.onmouseout = btt_hideTooltip;
	el.onmousemove = btt_Locate;
}

function btt_showTooltip(e)
{
	document.getElementById("btc").appendChild(this.tooltip);
	btt_Locate(e);
}

function btt_hideTooltip(e)
{
	var d = document.getElementById("btc");
	if (d.childNodes.length > 0) {
		d.removeChild(d.firstChild);
	}
}

function btt_setOpacity(el)
{
	el.style.filter = "alpha(opacity:95)";
	el.style.KHTMLOpacity = "0.95";
	el.style.MozOpacity = "0.95";
	el.style.opacity = "0.95";
}

function btt_CreateEl(t, c)
{
	var x = document.createElement(t);
	x.className = c;
	x.style.display = "block";
	return (x);
}

function btt_AddCss()
{
	var l = btt_CreateEl("link");
	l.setAttribute("type", "text/css");
	l.setAttribute("rel", "stylesheet");
	l.setAttribute("href", "static/bt.css");
	l.setAttribute("media", "screen");
	document.getElementsByTagName("head")[0].appendChild(l);
}

function btt_Locate(e)
{
	var posx = 0, posy = 0;
	if (e == null) {
		e = window.event;
	}
	if (e.pageX || e.pageY) {
		posx = e.pageX;
		posy = e.pageY;
	}
	
	else if (e.clientX || e.clientY) {
		if (document.documentElement.scrollTop) {
			posx =
			    e.clientX +
			    document.documentElement.scrollLeft;
			posy =
			    e.clientY + document.documentElement.scrollTop;
		}
		
		else {
			posx = e.clientX + document.body.scrollLeft;
			posy = e.clientY + document.body.scrollTop;
		}
	}
	document.getElementById("btc").style.top = (posy + 10) + "px";
	document.getElementById("btc").style.left = (posx - 260) + "px";
}
