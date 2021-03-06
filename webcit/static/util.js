// small but works-for-me stuff for testing javascripts
// not ready for "production" use

Object.inspect = function(obj) {
  var info = [];
  
  if(typeof obj in ["string","number"]) {
    return obj;
  } else {
    for(property in obj)
      if(typeof obj[property]!="function")
        info.push(property + ' => ' + 
          (typeof obj[property] == "string" ?
            '"' + obj[property] + '"' :
            obj[property]));
  }
  
  return ("'" + obj + "' #" + typeof obj + 
    ": {" + info.join(", ") + "}");
}

// borrowed from http://www.schuerig.de/michael/javascript/stdext.js
// Copyright (c) 2005, Michael Schuerig, michael@schuerig.de
// License
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the 
// See http://www.gnu.org/copyleft/lesser.html

Array.flatten = function(array, excludeUndefined) {
  if (excludeUndefined === undefined) {
    excludeUndefined = false;
  }
  var result = [];
  var len = array.length;
  for (var i = 0; i < len; i++) {
    var el = array[i];
    if (el instanceof Array) {
      var flat = el.flatten(excludeUndefined);
      result = result.concat(flat);
    } else if (!excludeUndefined || el != undefined) {
      result.push(el);
    }
  }
  return result;
};

if (!Array.prototype.flatten) {
  Array.prototype.flatten = function(excludeUndefined) {
    return Array.flatten(this, excludeUndefined);
  }
}

/*--------------------------------------------------------------------------*/

var Builder = {
  node: function(elementName) {
    var element = document.createElement('div');
    element.innerHTML = 
      "<" + elementName + "></" + elementName + ">";

    // attributes (or text)
    if(arguments[1])
      if(this._isStringOrNumber(arguments[1]) ||
        (arguments[1] instanceof Array)) {
          this._children(element.firstChild, arguments[1]);
        } else {
          var attrs = this._attributes(arguments[1]);
          if(attrs.length) 
            element.innerHTML = "<" +elementName + " " +
              attrs + "></" + elementName + ">";
        } 

    // text, or array of children
    if(arguments[2])
      this._children(element.firstChild, arguments[2]);

     return element.firstChild;
  },
  _text: function(text) {
     return document.createTextNode(text);
  },
  _attributes: function(attributes) {
    var attrs = [];
    for(attribute in attributes)
      attrs.push((attribute=='className' ? 'class' : attribute) +
          '="' + attributes[attribute].toString().escapeHTML() + '"');
    return attrs.join(" ");
  },
  _children: function(element, children) {
    if(typeof children=='object') { // array can hold nodes and text
      children = children.flatten();
      for(var i = 0; i<children.length; i++)
        if(typeof children[i]=='object')
          element.appendChild(children[i]);
        else
          if(this._isStringOrNumber(children[i]))
            element.appendChild(this._text(children[i]));
    } else
      if(this._isStringOrNumber(children)) 
         element.appendChild(this._text(children));
  },
  _isStringOrNumber: function(param) {
    return(typeof param=='string' || typeof param=='number');
  }
}

/* ------------- element ext -------------- */

// adapted from http://dhtmlkitchen.com/learn/js/setstyle/index4.jsp
// note: Safari return null on elements with display:none; see http://bugzilla.opendarwin.org/show_bug.cgi?id=4125
// instead of "auto" values returns null so it's easier to use with || constructs

String.prototype.camelize = function() {
  var oStringList = this.split('-');
  if(oStringList.length == 1)    
    return oStringList[0];
  var ret = this.indexOf("-") == 0 ? 
    oStringList[0].charAt(0).toUpperCase() + oStringList[0].substring(1) : oStringList[0];
  for(var i = 1, len = oStringList.length; i < len; i++){
    var s = oStringList[i];
    ret += s.charAt(0).toUpperCase() + s.substring(1)
  }
  return ret;
}

Element.getStyle = function(element, style) {
  element = $(element);
  var value = element.style[style.camelize()];
  if(!value)
    if(document.defaultView && document.defaultView.getComputedStyle) {
      var css = document.defaultView.getComputedStyle(element, null);
      value = (css!=null) ? css.getPropertyValue(style) : null;
    } else if(element.currentStyle) {
      value = element.currentStyle[style.camelize()];  
    }
  if(value=='auto') value = null;
  return value;
}

Element.makePositioned = function(element) {
  element = $(element);
  if(Element.getStyle(element, 'position')=='static')
    element.style.position = "relative";
}

Element.makeClipping = function(element) {
  element = $(element);
  element._overflow = Element.getStyle(element, 'overflow') || 'visible';
  if(element._overflow!='hidden') element.style.overflow = 'hidden';
}

Element.undoClipping = function(element) {
  element = $(element);
  if(element._overflow!='hidden') element.style.overflow = element._overflow;
}

Element.collectTextNodesIgnoreClass = function(element, ignoreclass) {
  var children = $(element).childNodes;
  var text     = "";
  var classtest = new RegExp("^([^ ]+ )*" + ignoreclass+ "( [^ ]+)*$","i");

  for (var i = 0; i < children.length; i++) {
    if(children[i].nodeType==3) {
      text+=children[i].nodeValue;
    } else {
      if((!children[i].className.match(classtest)) && children[i].hasChildNodes())
        text += Element.collectTextNodesIgnoreClass(children[i], ignoreclass);
    }
  }

  return text;
}

/*--------------------------------------------------------------------------*/

Position.positionedOffset = function(element) {
  var valueT = 0, valueL = 0;
  do {
    valueT += element.offsetTop  || 0;
    valueL += element.offsetLeft || 0;
    element = element.offsetParent;
    if (element) {
      p = Element.getStyle(element,'position');
      if(p == 'relative' || p == 'absolute') break;
    }
  } while (element);
  return [valueL, valueT];
}

// Safari returns margins on body which is incorrect if the child is absolutely positioned.
// for performance reasons, we create a specialized version of Position.positionedOffset for
// KHTML/WebKit only

if(/Konqueror|Safari|KHTML/.test(navigator.userAgent)) {
  Position.cumulativeOffset = function(element) {
    var valueT = 0, valueL = 0;
    do {
      valueT += element.offsetTop  || 0;
      valueL += element.offsetLeft || 0;
      
      if (element.offsetParent==document.body) 
        if (Element.getStyle(element,'position')=='absolute') break;
        
      element = element.offsetParent;
    } while (element);
    return [valueL, valueT];
  }
}

Position.page = function(forElement) {
  if(element == document.body) return [0, 0];
  var valueT = 0, valueL = 0;

  var element = forElement;
  do {
    valueT += element.offsetTop  || 0;
    valueL += element.offsetLeft || 0;

    // Safari fix
    if (element.offsetParent==document.body)
      if (Element.getStyle(element,'position')=='absolute') break;
      
  } while (element = element.offsetParent);

  element = forElement;
  do {
    valueT -= element.scrollTop  || 0;
    valueL -= element.scrollLeft || 0;    
  } while (element = element.parentNode);

  return [valueL, valueT];
}

// elements with display:none don't return an offsetParent, 
// fall back to  manual calculation
Position.offsetParent = function(element) {
  if(element.offsetParent) return element.offsetParent;
  if(element == document.body) return element;
  
  while ((element = element.parentNode) && element != document.body)
    if (Element.getStyle(element,'position')!='static')
      return element;
  
  return document.body;
}

Position.clone = function(source, target) {
  var options = Object.extend({
    setLeft:    true,
    setTop:     true,
    setWidth:   true,
    setHeight:  true,
    offsetTop:  0,
    offsetLeft: 0
  }, arguments[2] || {})
  
  // find page position of source
  source = $(source);
  var p = Position.page(source);

  // find coordinate system to use
  target = $(target);
  var delta = [0, 0];
  var parent = null;
  // delta [0,0] will do fine with position: fixed elements, 
  // position:absolute needs offsetParent deltas
  if (Element.getStyle(target,'position') == 'absolute') {
    parent = Position.offsetParent(target);
    delta = Position.page(parent);
  }
  
  // correct by body offsets (fixes Safari)
  if (parent==document.body) {
    delta[0] -= document.body.offsetLeft;
    delta[1] -= document.body.offsetTop; 
  }

  // set position
  if(options.setLeft)   target.style.left  = (p[0] - delta[0] + options.offsetLeft) + "px";
  if(options.setTop)    target.style.top   = (p[1] - delta[1] + options.offsetTop) + "px";
  if(options.setWidth)  target.style.width = source.offsetWidth + "px";
  if(options.setHeight) target.style.height = source.offsetHeight + "px";
}

Position.absolutize = function(element) {
  element = $(element);
  if(element.style.position=='absolute') return;
  Position.prepare();

  var offsets = Position.positionedOffset(element);
  var top     = offsets[1];
  var left    = offsets[0];
  var width   = element.clientWidth;
  var height  = element.clientHeight;

  element._originalLeft   = left - parseFloat(element.style.left  || 0);
  element._originalTop    = top  - parseFloat(element.style.top || 0);
  element._originalWidth  = element.style.width;
  element._originalHeight = element.style.height;

  element.style.position = 'absolute';
  element.style.top    = top + 'px';;
  element.style.left   = left + 'px';;
  element.style.width  = width + 'px';;
  element.style.height = height + 'px';;
}

Position.relativize = function(element) {
  element = $(element);
  if(element.style.position=='relative') return;
  Position.prepare();

  element.style.position = 'relative';
  var top  = parseFloat(element.style.top  || 0) - (element._originalTop || 0);
  var left = parseFloat(element.style.left || 0) - (element._originalLeft || 0);

  element.style.top    = top + 'px';
  element.style.left   = left + 'px';
  element.style.height = element._originalHeight;
  element.style.width  = element._originalWidth;
}

/*--------------------------------------------------------------------------*/

Element.Class = {
    // Element.toggleClass(element, className) toggles the class being on/off
    // Element.toggleClass(element, className1, className2) toggles between both classes,
    //   defaulting to className1 if neither exist
    toggle: function(element, className) {
      if(Element.Class.has(element, className)) {
        Element.Class.remove(element, className);
        if(arguments.length == 3) Element.Class.add(element, arguments[2]);
      } else {
        Element.Class.add(element, className);
        if(arguments.length == 3) Element.Class.remove(element, arguments[2]);
      }
    },

    // gets space-delimited classnames of an element as an array
    get: function(element) {
      element = $(element);
      return element.className.split(' ');
    },

    // functions adapted from original functions by Gavin Kistner
    remove: function(element) {
      element = $(element);
      var regEx;
      for(var i = 1; i < arguments.length; i++) {
        regEx = new RegExp("(^|\\s)" + arguments[i] + "(\\s|$)", 'g');
        element.className = element.className.replace(regEx, '')
      }
    },

    add: function(element) {
      element = $(element);
      for(var i = 1; i < arguments.length; i++) {
        Element.Class.remove(element, arguments[i]);
        element.className += (element.className.length > 0 ? ' ' : '') + arguments[i];
      }
    },

    // returns true if all given classes exist in said element
    has: function(element) {
      element = $(element);
      if(!element || !element.className) return false;
      var regEx;
      for(var i = 1; i < arguments.length; i++) {
        if((typeof arguments[i] == 'object') && 
          (arguments[i].constructor == Array)) {
          for(var j = 0; j < arguments[i].length; j++) {
            regEx = new RegExp("(^|\\s)" + arguments[i][j] + "(\\s|$)");
            if(!regEx.test(element.className)) return false;
          }
        } else {
          regEx = new RegExp("(^|\\s)" + arguments[i] + "(\\s|$)");
          if(!regEx.test(element.className)) return false;
        }
      }
      return true;
    },

    // expects arrays of strings and/or strings as optional paramters
    // Element.Class.has_any(element, ['classA','classB','classC'], 'classD')
    has_any: function(element) {
      element = $(element);
      if(!element || !element.className) return false;
      var regEx;
      for(var i = 1; i < arguments.length; i++) {
        if((typeof arguments[i] == 'object') && 
          (arguments[i].constructor == Array)) {
          for(var j = 0; j < arguments[i].length; j++) {
            regEx = new RegExp("(^|\\s)" + arguments[i][j] + "(\\s|$)");
            if(regEx.test(element.className)) return true;
          }
        } else {
          regEx = new RegExp("(^|\\s)" + arguments[i] + "(\\s|$)");
          if(regEx.test(element.className)) return true;
        }
      }
      return false;
    },

    childrenWith: function(element, className) {
      var children = $(element).getElementsByTagName('*');
      var elements = new Array();

      for (var i = 0; i < children.length; i++) {
        if (Element.Class.has(children[i], className)) {
          elements.push(children[i]);
          break;
        }
      }

      return elements;
    }
}

/*--------------------------------------------------------------------------*/

String.prototype.parseQuery = function() {
  var str = this;
  if(str.substring(0,1) == '?') {
    str = this.substring(1);
  }
  var result = {};
  var pairs = str.split('&');
  for(var i = 0; i < pairs.length; i++) {
    var pair = pairs[i].split('=');
    result[pair[0]] = pair[1];
  }
  return result;
}