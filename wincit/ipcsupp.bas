Attribute VB_Name = "IPCSUPP"
Global tcpconnected
Global count_serv_puts&
Global InTrans
Global TCPinbuf$

Type BBSdir
    Name As String * 80
    PhoneOrAddress As String * 50
    TCPport As Integer
    End Type

Global CurrBBS As BBSdir
Global editedBBSnum As Integer

Declare Function GetPrivateProfileInt Lib "kernel32" Alias "GetPrivateProfileIntA" (ByVal lpApplicationName As String, ByVal lpKeyName As String, ByVal nDefault As Long, ByVal lpFileName As String) As Long
Declare Function GetPrivateProfileString Lib "kernel32" Alias "GetPrivateProfileStringA" (ByVal lpApplicationName As String, ByVal lpKeyName As Any, ByVal lpDefault As String, ByVal lpReturnedString As String, ByVal nSize As Long, ByVal lpFileName As String) As Long
Declare Function WritePrivateProfileString Lib "kernel32" Alias "WritePrivateProfileStringA" (ByVal lpApplicationName As String, ByVal lpKeyName As Any, ByVal lpString As Any, ByVal lpFileName As String) As Long



Function begin_trans() As Integer
    
    If InTrans = True Then
        begin_trans = False
    Else
        InTrans = True
        MainWin.MousePointer = 11
        begin_trans = True
        End If
End Function




Sub end_trans()
    MainWin.MousePointer = 0
    InTrans = False
End Sub

Function GetPrivateProfileVBString(lpApplicationName As String, lpKeyName As String, nDefault As String, ByVal lpFileName As String) As String

    buf$ = "                                                                     "
    a% = GetPrivateProfileString(lpApplicationName, lpKeyName, nDefault, buf$, Len(buf$), lpFileName)

    
    GetPrivateProfileVBString = StripTrailingWhiteSpace(buf$)
End Function


Function serv_gets()
    buf$ = ""

        Do
            DoEvents
            Loop While InStr(TCPinbuf$, Chr$(10)) = 0
        b% = InStr(TCPinbuf$, Chr$(10))
        serv_gets = Left$(TCPinbuf$, b% - 1)
        TCPinbuf$ = Right$(TCPinbuf$, Len(TCPinbuf$) - b%)

End Function

Sub serv_puts(buf$)
    IPC.TCP.Send (buf$ + Chr$(10))
End Sub

Function serv_read(bytes As Integer)
    buf$ = ""

        Do
            DoEvents
            Loop While Len(TCPinbuf$) < bytes
        serv_read = Left$(TCPinbuf$, bytes)
        TCPinbuf$ = Right$(TCPinbuf$, Len(TCPinbuf$) - bytes)


End Function

Sub Transmit_Buffer(SendBuf As String)
            a$ = SendBuf
            Do Until Len(a$) = 0
                nl% = InStr(a$, Chr$(13) + Chr$(10))
                If (nl% > 0) And (nl% < 200) Then
                    serv_puts (Left$(a$, nl% - 1))
                    a$ = " " + Right$(a$, Len(a$) - nl% - 1)
                    End If
                If (nl% = 0) And (Len(a$) < 200) Then
                    serv_puts (a$)
                    a$ = ""
                    End If
                If ((nl% = 0) And (Len(a$) >= 200)) Or (nl% >= 200) Then
                    ns% = 0
                    For z = 1 To Len(a$)
                        y = InStr(z, a$, " ")
                        If (y > ns%) Then ns% = y
                        Next z
                    If (ns% = 0) Or (ns% >= 200) Then ns% = 200
                    serv_puts (Left$(a$, ns% - 1))
                    a$ = Right$(a$, Len(a$) - ns%)
                    End If
                Loop

End Sub

