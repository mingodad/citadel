/** Because scriptaculous DnD sucks..
    Written by Mathew McBride <matt@mcbridematt.dhs.org> / <matt@comalies>
*/
var draggedElement = null;
var currentDropTargets = null;
var dropTarget = null;
function mouseDownHandler(event) {
  var target = event.target;
  var actualTarget = target;
  if (target.nodeName.toLowerCase() == "span") {
    actualTarget = target.parentNode;
  }
  if (!actualTarget.dropEnabled) {
    return;
  }
  turnOffTextSelect();
  draggedElement = actualTarget;
  draggedElement.originalPosition = draggedElement.style.position;
  return false;
}
function mouseUpHandler(event) {
  var target = event.target;
  var dropped = dropTarget;
  if (draggedElement != null) {
  if (dropped != null && dropped.dropHandler) {
    dropped.dropHandler(dropped,draggedElement);
  }
  draggedElement.style.position = draggedElement.originalPosition;
  draggedElement.style.top = "";
  draggedElement.style.left = "";
  draggedElement.style.border = "";
  draggedElement = null;
  dropTarget = null;
  }
  turnOnTextSelect();
  return true;
}
function mouseMoveHandler(event) {
  if (draggedElement != null) {
  var clientX = event.clientX+5;
  var clientY = event.clientY+5;
  draggedElement.style.position = "fixed";
  draggedElement.style.top = clientY + "px";
  draggedElement.style.left = clientX + "px";
  draggedElement.style.border = "1px solid black";
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
document.observe("dom:loaded", setupDragDrop);
function setupDragDrop() {
if (document.addEventListener != undefined) {
     $(document.body).observe('mousedown', mouseDownHandler);
    $(document.body).observe('mouseup',mouseUpHandler);
    $(document.body).observe('mousemove',mouseMoveHandler);
    $(document.body).observe('mouseover', mouseMoveOver);
    $(document.body).observe('mouseout', mouseMoveOut);
    } 
}
function turnOffTextSelect() {
  document.onmousedown = new Function("return false");
document.onmouseup = new Function("return true");
}
function turnOnTextSelect() {
  document.onmousedown = null;
  document.onmouseup = null;
}
