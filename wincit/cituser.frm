VERSION 4.00
Begin VB.Form CitUser 
   AutoRedraw      =   -1  'True
   BorderStyle     =   1  'Fixed Single
   ClientHeight    =   6600
   ClientLeft      =   1140
   ClientTop       =   1500
   ClientWidth     =   9255
   ControlBox      =   0   'False
   BeginProperty Font 
      name            =   "MS Sans Serif"
      charset         =   0
      weight          =   700
      size            =   8.25
      underline       =   0   'False
      italic          =   0   'False
      strikethrough   =   0   'False
   EndProperty
   ForeColor       =   &H80000008&
   Height          =   7005
   Left            =   1080
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6600
   ScaleWidth      =   9255
   Top             =   1155
   Width           =   9375
   Begin Threed.SSPanel helloframe 
      Height          =   3375
      Left            =   120
      TabIndex        =   11
      Top             =   3120
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   5953
      _StockProps     =   15
      BackColor       =   12632256
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
      Begin VB.TextBox hello 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   3135
         Left            =   120
         MultiLine       =   -1  'True
         TabIndex        =   12
         Top             =   120
         Width           =   6015
      End
   End
   Begin Threed.SSPanel Panel3D1 
      Height          =   1935
      Left            =   6480
      TabIndex        =   10
      Top             =   2160
      Width           =   2655
      _Version        =   65536
      _ExtentX        =   4683
      _ExtentY        =   3413
      _StockProps     =   15
      BackColor       =   12632256
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin Threed.SSPanel Instructions 
      Height          =   2295
      Left            =   6480
      TabIndex        =   1
      Top             =   4200
      Width           =   2655
      _Version        =   65536
      _ExtentX        =   4683
      _ExtentY        =   4048
      _StockProps     =   15
      Caption         =   $"CITUSER.frx":0000
      BackColor       =   12632256
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin VB.CommandButton newuser_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "New User"
      Height          =   375
      Left            =   6480
      TabIndex        =   9
      Top             =   1680
      Width           =   1335
   End
   Begin VB.CommandButton disc_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Disconnect"
      Height          =   375
      Left            =   7920
      TabIndex        =   5
      Top             =   1680
      Width           =   1215
   End
   Begin VB.CommandButton login_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Log In"
      Height          =   375
      Left            =   6480
      TabIndex        =   4
      Top             =   1200
      Width           =   1335
   End
   Begin VB.TextBox Password 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "Wingdings"
         charset         =   2
         weight          =   700
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   525
      Left            =   240
      TabIndex        =   3
      Top             =   2400
      Width           =   6015
   End
   Begin VB.TextBox UserName 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   18
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Left            =   240
      TabIndex        =   0
      Top             =   1440
      Width           =   6015
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   855
      Left            =   120
      TabIndex        =   8
      Top             =   2160
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Password:"
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   855
      Left            =   120
      TabIndex        =   2
      Top             =   1200
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "User Name:"
   End
   Begin VB.Label BBScity 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C00000&
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H0000FFFF&
      Height          =   375
      Left            =   120
      TabIndex        =   7
      Top             =   720
      Width           =   9015
   End
   Begin VB.Label BBSname 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C00000&
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   18
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H0000FFFF&
      Height          =   495
      Left            =   120
      TabIndex        =   6
      Top             =   120
      Width           =   9015
   End
End
Attribute VB_Name = "CitUser"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub check_buttons()
    If UserName.Text <> "" And Password.Text <> "" Then
        login_button.Enabled = True
        newuser_button.Enabled = True
    Else
        login_button.Enabled = False
        newuser_button.Enabled = False
        End If
End Sub

Private Sub disc_button_Click()
    

        Call serv_puts("QUIT")
        dummy$ = serv_gets()

    Unload CitUser
    Unload IPC
    Load SelectBBS

End Sub

Private Sub form_activate()
    CitUser.WindowState = 0
    CitUser.Left = Int((MainWin.Width - CitUser.Width) / 2)
    CitUser.Top = Int((MainWin.Height - CitUser.Height) / 3)
End Sub

Private Sub Form_Load()
    CitUser.WindowState = 0
    CitUser.Left = Int((MainWin.Width - CitUser.Width) / 2)
    CitUser.Top = Int((MainWin.Height - CitUser.Height) / 3)
    Call check_buttons
    Show
    SaveOldCount = 0
    SaveNewCount = 0
    CurrRoomName$ = ""

    ' Use the INFO command to retrieve global server information
    ' (This could probably get moved to the IPC module)
    '
    If begin_trans() = False Then GoTo skipcitu

    serv_puts ("IDEN 0|2|100|WinCit")
    buf$ = serv_gets()

    serv_puts ("INFO")
    buf$ = serv_gets()
    If Left$(buf$, 1) = "1" Then
        a% = 0
        buf$ = ""
        Do
            buf$ = serv_gets()
            If buf$ = "000" Then Exit Do
            a% = a% + 1
            Select Case a%
                Case 1
                    serv_pid% = Val(buf$)
                Case 2
                    serv_nodename$ = buf$
                Case 3
                    serv_humannode$ = buf$
                Case 4
                    serv_fqdn$ = buf$
                Case 5
                    serv_software$ = buf$
                Case 6
                    serv_rev_level! = CDbl(buf$) / 100
                Case 7
                    serv_bbs_city$ = buf$
                Case 8
                    serv_sysadm$ = buf$
                End Select
            Loop
        Call end_trans
        BBSname.Caption = serv_humannode$
        BBScity.Caption = serv_bbs_city$
        End If

    MainWin.Caption = serv_humannode$
skipcitu:
    
    If begin_trans() = False Then GoTo skiphello
    serv_puts ("MESG hello")
    c$ = serv_gets()
    If (Left$(c$, 1) = "1") Then
        b$ = ""
        Do
            c$ = serv_gets()
            If (c$ <> "000") Then
                Do While Left$(c$, 2) = "  "
                    c$ = Right$(c$, Len(c$) - 1)
                    Loop
                b$ = b$ + c$ + Chr$(13) + Chr$(10)
                End If
            Loop Until c$ = "000"
        hello.Text = Cit_Format(b$)
    Else
        hello.Text = c$
        End If
    Call end_trans


skiphello:
    Call form_activate
    CitUser.SetFocus
    UserName.SetFocus

End Sub

' begin_trans() will already have been called before get_uparms()
'
Private Sub get_uparms(loginstr As String)

    p$ = Right$(loginstr, Len(loginstr) - 4)
    axlevel% = Val(extract(p$, 1))

    serv_puts ("CHEK")
    a$ = serv_gets()
    If Left$(a$, 1) = "2" Then
        a$ = Right$(a$, Len(a$) - 4)
        m$ = ""
        nm% = Val(extract(a$, 0))
        If nm% = 1 Then m$ = m$ + "You have a new private message in Mail>" + Chr$(13) + Chr$(10)
        If nm% > 1 Then m$ = m$ + "You have " + Str$(nm%) + " new private messages in Mail>" + Chr$(13) + Chr$(10)

        If axlevel >= 6 And Val(extract(a$, 2)) > 0 Then m$ = m$ + "Users need validation" + Chr$(13) + Chr$(10)

        If Len(m$) > 0 Then MsgBox m$, 64
        
        need_regis% = Val(extract(a$, 1))

        End If



End Sub

Private Sub Login_Button_Click()
    
    If begin_trans() = True Then
        serv_puts ("USER " + UserName.Text)
        buf$ = serv_gets()
        Call end_trans
        If Left$(buf$, 1) <> "3" Then
            MsgBox Right$(buf$, Len(buf$) - 4), 16
            End If
        If Left$(buf$, 1) = "3" Then
            serv_puts ("PASS " + Password.Text)
            buf$ = serv_gets()
            If Left$(buf$, 1) <> "2" Then
                MsgBox Right$(buf$, Len(buf$) - 4), 16
                End If
            If Left$(buf$, 1) = "2" Then
                get_uparms (buf$)
                Unload CitUser
                If need_regis% = 1 Then
                    Load Registration
                Else
                    Load RoomPrompt
                    End If
                End If
            End If
        End If


End Sub

Private Sub newuser_button_Click()
    If begin_trans() = True Then
        serv_puts ("NEWU " + UserName.Text)
        buf$ = serv_gets()
        If Left$(buf$, 1) = "2" Then
            serv_puts ("SETP " + Password.Text)
            buf$ = serv_gets()
            Call end_trans
            If Left$(buf$, 1) <> "2" Then
                MsgBox Right$(buf$, Len(buf$) - 4), 16
                End If
            get_uparms (buf$)
            Unload CitUser
                If need_regis% = 1 Then
                    Load Registration
                Else
                    Load RoomPrompt
                    End If
        Else
            MsgBox Right$(buf$, Len(buf$) - 4), 16
            End If
        Call end_trans
        End If
End Sub

Private Sub Password_Change()
    Call check_buttons
End Sub

Private Sub Password_Click()

Rem

End Sub


Private Sub Password_KeyPress(keyascii As Integer)
    If keyascii = 13 Then Call Login_Button_Click
End Sub

Private Sub UserName_Change()
    Call check_buttons
End Sub

Private Sub UserName_KeyPress(keyascii As Integer)
    If keyascii = 13 Then Call Password_Click
End Sub

