/*
 * wcCitUtil.java
 *
 * Miscellaneous functionality...
 */

import java.util.*;


public class wcCitUtil extends Object {

public static String Extract(String SourceString, int ParmNum) {
	StringTokenizer toks;
	String buf;
	int pos;

	pos = 0;
	toks = new StringTokenizer(SourceString, "|");
	while (toks.hasMoreTokens()) {
		buf = toks.nextToken();
		if (pos == ParmNum) return buf;
		++pos;
		}
	return "";
	}

}
