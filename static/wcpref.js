/*
 * Copyright 2005 - 2009 The Citadel Team
 * Licensed under the GPL V3
 * Webcit preference code 
 */
var persistentStorage = false;
/* DOM5 storage disabled for now.. we want localStorage which isn't as widely available yet */
//if (window.sessionStorage) {
//  persistentStorage = true;
//}
function WCPrefs() {
  this.cookieValCache = new Object();
  this.noExpire = "Mon, 18 Jan 2038 5:14:07 AM";
}

function readPref(name) {
  if (persistentStorage) {
    return sessionStorage.getItem(name);
  } else {
    return this.cookieValCache[name];
  }
}

function setPref(name, value) {
  if (persistentStorage) {
    sessionStorage.setItem(name, value);
  } else {
    document.cookie = "WC_" + name + "="+value+";expires="+this.noExpire;
    //    this.cookieValCache[name] = value; 
    //this.saveLocal();
  }
}

function loadLocal() {
  if (!persistentStorage) {
    var cookies = document.cookie.split(";");
    for(var i=0; i<cookies.length; i++) {
      var cookie = cookies[i].split("=");
      var name = cookie[0];
      if (name.charAt(0) == " ") {
	name = name.substr(1);
      }
      if (name.substr(0, 3) == "WC_") {
	name = name.substr(3);
	this.cookieValCache[name] = cookie[1];
	if (!!window.console) {
	  console.log(name+"="+cookie[1]);
	}
      }
    }
  }
}

function saveLocal() {
  if (!persistentStorage) {
    // First we purge cookies with WC_ infront
    var cookies = document.cookie.split(";");
    var numOfCookies = cookies.length;
    var finalCookieString = "";
    for(var i=0; i<numOfCookies; i++) 
      var cookie = cookies[i].split("=");
      var name = cookie[0];
      if (name.substr(0, 3) != "WC_") {
	finalCookieString += cookies[i] + ";";
      }
    }
    for(key in this.cookieValCache) {
      var val = this.cookieValCache[key];
      finalCookieString += "WC_" + key+"="+val+";";
    }
    document.cookie = finalCookieString;
}

WCPrefs.prototype.saveLocal = saveLocal;
WCPrefs.prototype.loadLocal = loadLocal;
WCPrefs.prototype.setPref = setPref;
WCPrefs.prototype.readPref = readPref;
var ctdlLocalPrefs = null;
setupPrefEngine();
function setupPrefEngine() {
  ctdlLocalPrefs = new WCPrefs();
  ctdlLocalPrefs.loadLocal();
}
