/** 
 * Because scriptaculous DnD sucks..
 * Written by Mathew McBride <matt@mcbridematt.dhs.org> / <matt@comalies>
 * 
 * Copyright 2009 The Citadel Team
 * Licensed under the GPL V3
 */
var draggedElement = null;
var currentDropTargets = null;
var dropTarget = null;
var dragAndDropElement = null;
var oldSelectHandler = null;
function mouseDownHandler(event) {
  var target = event.target;
  var actualTarget = target;
  if (target.nodeName.toLowerCase() == "td") {
    actualTarget = target.parentNode;
  }
  if (!actualTarget.dropEnabled && actualTarget.getAttribute("citadel:dropenabled") == null) {
    return;
  }
  turnOffTextSelect();
  draggedElement = actualTarget;
  return false;
}
function mouseUpHandler(event) {
  var target = event.target;
  var dropped = dropTarget;
  if (dragAndDropElement != null) {
  if (dropped != null && dropped.dropHandler) {
    dropped.dropHandler(dropped,draggedElement);
  }
  document.body.removeChild(dragAndDropElement);
  }
  dragAndDropElement = null;
  draggedElement = null;
  dropTarget = null;
  turnOnTextSelect();
  return true;
}
function mouseMoveHandler(event) {
  if (draggedElement != null) {
    if (dragAndDropElement == null) {
      var dragAndDropElementFunction = (draggedElement.ctdlDnDElement) ? draggedElement.ctdlDndElement : eval(draggedElement.getAttribute("citadel:dndelement"));
      dragAndDropElement = dragAndDropElementFunction.call();
    dragAndDropElement.className = "draganddrop";
    document.body.appendChild(dragAndDropElement);
    }
    var clientX = event.clientX+5;
    var clientY = event.clientY+5;
    dragAndDropElement.style.top = clientY + "px";
    dragAndDropElement.style.left = clientX + "px";
  }
  return false;
}
function mouseMoveOver(event) {
  if (event.target.dropTarget) {
    dropTarget = event.target;
  }
}
function mouseMoveOut(event) {
  if (dropTarget) {
    dropTarget = null;
  }
}
function setupDragDrop() {
  $(document.body).observe('mousedown', mouseDownHandler);
    $(document.body).observe('mouseup',mouseUpHandler);
    $(document.body).observe('mousemove',mouseMoveHandler);
    $(document.body).observe('mouseover', mouseMoveOver);
    $(document.body).observe('mouseout', mouseMoveOut); 
}
function turnOffTextSelect() {
  document.onmousedown = new Function("return false");
  document.onmouseup = new Function("return true");
 oldSelectHandler = document.onselectstart;
 document.onselectstart = function() { return false; };
}
function turnOnTextSelect() {
  document.onmousedown = null;
  document.onmouseup = null;
  document.onselectstart = oldSelectHandler;
}
