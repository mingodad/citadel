Attribute VB_Name = "GLOBALS"
' KeepAlive is the frequency with which to send NOOP
' commands to the server to keep it from timing out
' (and also to check for express messages).

Global Const KeepAlive = 30000
Global Const MaxRooms = 200
Global Const DefaultPort = 2112

' Used during startup for user or system cancel of call
Global Cancelled

Global CurrRoomName$
Global CurrRoomFlags%
Global IsRoomAide%
Global DoubleClickAction$

Global SaveNewRooms$(MaxRooms)
Global SaveOldRooms$(MaxRooms)
Global SaveNewCount
Global SaveOldCount

Global serv_pid%
Global serv_nodename$
Global serv_humannode$
Global serv_fqdn$
Global serv_software$
Global serv_rev_level!
Global serv_bbs_city$
Global serv_sysadm$

Global msg_array&(256)
Global max_msgs%

Global recp$

Global axlevel%
Global need_regis%

Global axdefs$(7)
Global HoldMessage$

Global DownLoadFileName$

Global LastMessageRead&

Function Cit_Format(ib As String) As String

ob$ = ""
fl% = 0
Do Until Len(ib) = 0
    
    nl% = InStr(ib, Chr$(13) + Chr$(10))
    If nl% = 0 Then
        a$ = ib
        ib = ""
    Else
        a$ = Left$(ib, nl% - 1)
        ib = Right$(ib, Len(ib) - nl% - 1)
        End If

    If fl% = 1 And Left$(a$, 1) = " " Then
        ob$ = ob$ + Chr$(13) + Chr$(10)
        End If

    ob$ = ob$ + a$ + " "
    
    fl% = 1
    Loop

    Do While Left$(ob$, 2) = Chr$(13) + Chr$(10)
        ob$ = Right$(ob$, Len(ob$) - 2)
        Loop
    Do While Right$(ob$, 2) = Chr$(13) + Chr$(10)
        ob$ = Left$(ob$, Len(ob$) - 2)
        Loop

Cit_Format = ob$
End Function

Function extract$(source$, parmnum%)

    buf$ = source$
    If parmnum% > 0 Then
        For a% = 1 To parmnum%
            b% = InStr(1, buf$, "|")
            If (b% > 0) Then buf$ = Right$(buf$, Len(buf$) - b%)
            Next a%
        End If

    b% = InStr(1, buf$, "|")
    If b% > 0 Then buf$ = Left$(buf$, b% - 1)

    extract$ = buf$
End Function

Sub main()
    Call setvars        ' set unixtime stuff in Ford module
    editedBBSnum = (-1)
    
    axdefs$(0) = "Marked for deletion"
    axdefs$(1) = "New unvalidated user"
    axdefs$(2) = "Problem user"
    axdefs$(3) = "Local user"
    axdefs$(4) = "Network user"
    axdefs$(5) = "Preferred user"
    axdefs$(6) = "Aide"
    
    Load SelectBBS
End Sub

Function StripTrailingWhiteSpace(padstr As String) As String
    If Len(padstr) > 0 Then
        Do While Asc(Right$(padstr, 1)) = 0 Or Right$(padstr, 1) = " "
            padstr = Left$(padstr, Len(padstr) - 1)
            If Len(padstr) < 1 Then Exit Do
            Loop
        End If
    
    If Len(padstr) > 0 Then
        Do While Asc(Left$(padstr, 1)) = 0 Or Left$(padstr, 1) = " "
            padstr = Right$(padstr, Len(padstr) - 1)
            If Len(padstr) < 1 Then Exit Do
            Loop
        End If

    StripTrailingWhiteSpace = padstr
End Function

