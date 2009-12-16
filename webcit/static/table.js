var categories;
/** 
 * Task view table sorter
 * Written by Mathew McBride <matt@mcbridematt.dhs.org>
 * Copyright 2009 The Citadel Team
 * Licensed under the GPL V3
 */
function gatherCategoriesFromTable() {
	var tbody = document.getElementById("taskview");
	var childNodes = tbody.childNodes;
	for (i=0; i<=childNodes.length; i++) {
		var child = childNodes[i]; // Should be TR
		if (child != undefined && child.nodeName == "TR") {
			var childTds = child.getElementsByTagName("TD");
			if (childTds.length == 4) {
			var categoryTd = childTds[3];
			if (categoryTd != undefined) {
				// Get text child
				if (categoryTd.childNodes.length > 0 &&
					categoryTd.childNodes[0].nodeType == 3) {
						categories[categoryTd.childNodes[0].nodeValue]
							= categoryTd.childNodes[0].nodeValue;
					}
			}
		}
	}
}
}
function addCategoriesToSelector() {
	var selector = document.getElementById("selectcategory");
	for (description in categories) {
		var newOptionElement = document.createElement("option");
		newOptionElement.setAttribute("value", categories[description]);
		var text = document.createTextNode(categories[description]);
		newOptionElement.appendChild(text);
		selector.appendChild(newOptionElement);
	}
}
function filterCategories(event) {
	hideAllExistingRows();
	var selector = document.getElementById("selectcategory");
	var selected = selector.selectedIndex;
	var selectedCategory = selector.options[selected];
		var tbody = document.getElementById("taskview");
	var cat = selectedCategory.getAttribute("value");
	var nodesToUnhide = new Array();
	var curIndex = 0;
	// Hunt down all the rows with this category using XPath
	if (document.evaluate) { // Only if we can do so, of course 
		var debugText = "";
		var toEvaluate = null;
		if (cat != 'showall') {
		toEvaluate = "//tr[td='"+cat+"']";
		} else {
			toEvaluate = "//tr[td]";
		}
		var trNodes = document.evaluate(toEvaluate,
		document,
		null,
		XPathResult.ANY_TYPE,
		null);
	var trNode = trNodes.iterateNext();
	while(trNode) {
		debugText += "<br>"+trNode.nodeName;
		nodesToUnhide[curIndex++] = trNode;
		trNode = trNodes.iterateNext();
	} 
		
	}
	for (i=0;i<curIndex;i++) {
		nodesToUnhide[i].style.display = "table-row";
		if (((i-1) % 2) == 0) {
			nodesToUnhide[i].setAttribute("class","table-alt-row");
		}
	}
}
function hideAllExistingRows() {
	var nodes = new Array();
	var curIndex = 0;
	if (document.evaluate) { // Only if we can do so, of course 
		var debugText = "";
		var toEvaluate = "//tr/td";
		var tdNodes = document.evaluate(toEvaluate,
		document,
		null,
		XPathResult.ANY_TYPE,
		null);
	var tdNode = tdNodes.iterateNext();
	while(tdNode) {
		// Get parent
		var parent = tdNode.parentNode;
		nodes[curIndex++] = parent;
		tdNode = tdNodes.iterateNext();
	} 
	}
	for(i=0;i<curIndex;i++) {
		nodes[i].style.display = "none";
		nodes[i].removeAttribute("class");
	}
}

function taskViewActivate(event ) {
	// Do not run if not tasks, do not run without XPath3
	if (document.getElementById("taskview") != null && document.evaluate != null) {
	// var count = countRowsInTaskView();
	categories = new Object();
	gatherCategoriesFromTable();
	addCategoriesToSelector();
	
	$('selectcategory').observe('change', filterCategories);
	filterCategories(null); // color the rows first
	}
}

