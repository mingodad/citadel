Option Explicit
DefInt A-Z

Global dayo(13) As Integer
Global mnth(12) As String
Const FordConstant = 18032'/* 5 hours 32 seconds, 5 hours is GMT, 32 seconds, I have no idea. */

Sub humantime (ByVal l&, mo%, da%, yr%, h%, m%, s%)
  ' here art, pass it l  the unix time number, and it'll return the others
    Dim lm&, lh&, ld&, ll&
    Dim leap%
    Dim Craig%

    l = l - FordConstant ' account for GMT
    lm = Int(l / 60)
    s = (l - lm * 60)
    l = Int(l / 60)

    lh = Int(l / 60) ' was lm
    m = (l - lh * 60)
    l = Int(l / 60)

    ll = Int(l / 24)
    h = (l - ll * 24)
    l = Int(l / 24)

    lm = Int((l + 365) / 365)
    lm = Int(lm / 4)'; /* leap days */

    '/* l is now whole days left */
    yr = Int((l - lm) / 365)
    yr = yr + 1970

    If (yr Mod 4) = 0 Then
      leap = 1
    Else
      leap = 0
    End If

    ld = Int((l - lm) / 365)
    da = ((l - ld * 365) - lm) + 1

    '/* da is days in this year */

    For lm = 1 To 12 ' was 0
      If (leap And lm > 1) Then
        Craig = dayo(lm) + 1
      Else
        Craig = dayo(lm)
      End If
      If (da <= Craig) Then
          ld = dayo(lm)
          mo = lm
          Exit For
      End If
    Next lm

    If (mo > 1) Then
      If (leap) And (mo > 2) Then
        da = da - (dayo(mo - 1) + 1)
      Else
        da = da - dayo(mo - 1)
      End If
    End If

End Sub

Sub setvars ()
  
  'setup for unixcalc
  dayo(0) = 0
  dayo(1) = 31
  dayo(2) = 59
  dayo(3) = 90
  dayo(4) = 120
  dayo(5) = 151
  dayo(6) = 181
  dayo(7) = 212
  dayo(8) = 243
  dayo(9) = 273
  dayo(10) = 304
  dayo(11) = 334
  dayo(12) = 365

  mnth(1) = "Jan"
  mnth(2) = "Feb"
  mnth(3) = "Mar"
  mnth(4) = "Apr"
  mnth(5) = "May"
  mnth(6) = "Jun"
  mnth(7) = "Jul"
  mnth(8) = "Aug"
  mnth(9) = "Sep"
  mnth(10) = "Oct"
  mnth(11) = "Nov"
  mnth(12) = "Dec"
  
End Sub

Function strtime (t As Long) As String
  'given a unixtime make a good string out of it
  Dim s As String
  Dim xmo%
  Dim xda%
  Dim xyr%
  Dim xh%
  Dim xm%
  Dim xs%
  Dim ap As String
  Dim tm$
  
  Call humantime(t, xmo, xda, xyr, xh, xm, xs)
  s = mnth(xmo) + " " + Trim$(Str$(xda)) + ", " + Trim$(Str$(xyr))
  If xh >= 12 Then
    ap = "pm"
    xh = xh - 12
  Else
    ap = "am"
  End If
  If xh = 0 Then xh = 12
  tm$ = Trim$(Str$(xm))
  If (xm < 10) Then tm$ = "0" + tm$
  s = s + " " + Trim$(Str$(xh)) + ":" + tm$ + ap
  strtime = s
End Function

Function unixtime (mo%, da%, yr%, hr%, min%, sec%) As Long
  'art, this is the other way, pass it month, day year ..., and it
  'returns the long int

  Dim yr2%, leap%
  Dim ret As Long, first As Long, retl As Long

  yr2 = yr - 1970
  '/* leap year is divisible by four except every 4 hunred years, don't worry about it
  '   we'll just die in year 2000, it happens to be divisible by 400 */
  leap = 0
  If (yr Mod 4 = 0) Then
      leap = 1'; /* leap a bit wrong 12/31/88 to 1/1/89 */
  End If

  first = yr2 * 365
  first = first + ((yr2 + 1) \ 4)

  first = first + dayo(mo - 1)' /* add up days in this year so far */
  If ((leap = 1) And (mo > 2)) Then
      first = first + 1
  End If
  
  first = first + da'; /* add the days in this month */
  first = first - 1';/* don't count today's seconds */

  '/* first is number of days go to seconds */
  ret = (60 * 60)
  ret = ret * 24
  ret = ret * first'; /* number of seconds from 1970 to last nite */
  retl = 60 * 60
  retl = retl * hr
  ret = ret + retl
  ret = ret + (60 * min)
  ret = ret + sec
  ret = ret + FordConstant'; /* I have no idea */
  unixtime = ret
End Function

