VERSION 4.00
Begin VB.Form ReadMessages 
   BorderStyle     =   0  'None
   ClientHeight    =   6915
   ClientLeft      =   1650
   ClientTop       =   1980
   ClientWidth     =   9600
   ControlBox      =   0   'False
   BeginProperty Font 
      name            =   "MS Serif"
      charset         =   0
      weight          =   700
      size            =   6.75
      underline       =   0   'False
      italic          =   0   'False
      strikethrough   =   0   'False
   EndProperty
   ForeColor       =   &H80000008&
   Height          =   7320
   Left            =   1590
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6915
   ScaleWidth      =   9600
   ShowInTaskbar   =   0   'False
   Top             =   1635
   Width           =   9720
   Begin VB.CommandButton print_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Print"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   2880
      TabIndex        =   19
      Top             =   6480
      Width           =   735
   End
   Begin VB.CommandButton quote_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Quote"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   3720
      TabIndex        =   18
      Top             =   6480
      Width           =   735
   End
   Begin VB.CommandButton Move_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Move"
      Enabled         =   0   'False
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   1200
      TabIndex        =   17
      Top             =   6480
      Width           =   735
   End
   Begin VB.CommandButton Delete_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Delete"
      Enabled         =   0   'False
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   2040
      TabIndex        =   16
      Top             =   6480
      Width           =   735
   End
   Begin Threed.SSPanel msg_num 
      Height          =   375
      Left            =   120
      TabIndex        =   15
      Top             =   6480
      Width           =   975
      _Version        =   65536
      _ExtentX        =   1720
      _ExtentY        =   661
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin Threed.SSPanel message_panel 
      Height          =   5535
      Left            =   0
      TabIndex        =   14
      Top             =   840
      Width           =   10455
      _Version        =   65536
      _ExtentX        =   18441
      _ExtentY        =   9763
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   9
      BorderWidth     =   0
      BevelOuter      =   1
      Begin VB.TextBox message_text 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         BeginProperty Font 
            name            =   "Courier New"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   5295
         Left            =   120
         Locked          =   -1  'True
         MultiLine       =   -1  'True
         ScrollBars      =   2  'Vertical
         TabIndex        =   3
         Top             =   120
         Width           =   10215
      End
   End
   Begin VB.CommandButton first_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "<<- &First"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   4560
      TabIndex        =   13
      Top             =   6480
      Width           =   855
   End
   Begin VB.CommandButton last_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Last ->>"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   7680
      TabIndex        =   12
      Top             =   6480
      Width           =   855
   End
   Begin Threed.SSPanel room_panel 
      Height          =   375
      Left            =   6000
      TabIndex        =   11
      Top             =   420
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   661
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin Threed.SSPanel date_panel 
      Height          =   375
      Left            =   6000
      TabIndex        =   8
      Top             =   15
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   661
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin Threed.SSPanel to_panel 
      Height          =   375
      Left            =   480
      TabIndex        =   7
      Top             =   420
      Width           =   4935
      _Version        =   65536
      _ExtentX        =   8705
      _ExtentY        =   661
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin VB.CommandButton back_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "<- &Back"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   5520
      TabIndex        =   2
      Top             =   6480
      Width           =   975
   End
   Begin VB.CommandButton next_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Next  ->"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   6600
      TabIndex        =   1
      Top             =   6480
      Width           =   975
   End
   Begin VB.CommandButton stop_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Stop"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   8640
      TabIndex        =   0
      Top             =   6480
      Width           =   855
   End
   Begin Threed.SSPanel from_panel 
      Height          =   375
      Left            =   480
      TabIndex        =   4
      Top             =   15
      Width           =   4935
      _Version        =   65536
      _ExtentX        =   8705
      _ExtentY        =   661
      _StockProps     =   15
      BackColor       =   12632256
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin VB.Label Label5 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Room:"
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
      Height          =   255
      Left            =   5400
      TabIndex        =   10
      Top             =   480
      Width           =   615
   End
   Begin VB.Label Label4 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Date:"
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
      Height          =   255
      Left            =   5520
      TabIndex        =   9
      Top             =   120
      Width           =   495
   End
   Begin VB.Label Label3 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "To:"
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
      Height          =   255
      Left            =   0
      TabIndex        =   6
      Top             =   480
      Width           =   495
   End
   Begin VB.Label Label2 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "From:"
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
      Height          =   255
      Left            =   0
      TabIndex        =   5
      Top             =   120
      Width           =   495
   End
End
Attribute VB_Name = "ReadMessages"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim curr_msg%
Dim varifontname
Dim varifontsize
Dim fixedfontname

Private Sub back_button_Click()
    
    curr_msg% = curr_msg% - 1
    Call display_msg

End Sub

Private Sub check_quote_button()
    If message_text.SelLength > 0 Then
        Quote_Button.Enabled = True
    Else
        Quote_Button.Enabled = False
        End If
End Sub

Private Sub Delete_Button_Click()
    If begin_trans() = False Then GoTo skipdelete
    serv_puts ("DELE " + Str$(msg_array&(curr_msg%)))
    a$ = serv_gets()
    Call end_trans

    If (Left$(a$, 1) = "2") Then
        For b% = curr_msg% To max_msgs% - 2
            msg_array&(b%) = msg_array(b% + 1)
            Next b%
        max_msgs% = max_msgs% - 1
        If max_msgs% = 0 Then Unload ReadMessages
        If curr_msg% >= (max_msgs% - 1) Then curr_msg% = curr_msg% - 1
        Call display_msg
    Else
        MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
        End If

skipdelete:
End Sub

Private Sub display_msg()

    If curr_msg% = 0 Then
        back_button.Enabled = False
        first_button.Enabled = False
    Else
        back_button.Enabled = True
        first_button.Enabled = True
        End If

    If curr_msg% = (max_msgs% - 1) Then
        next_button.Enabled = False
        last_button.Enabled = False
    Else
        next_button.Enabled = True
        last_button.Enabled = True
        End If
    
    msg_num.Caption = Str$(curr_msg% + 1) + " of " + Str$(max_msgs%)
    If begin_trans() = False Then GoTo skipitall
    LastMessageRead& = msg_array&(curr_msg%)
    serv_puts ("MSG0 " + Str$(msg_array&(curr_msg%)))
    a$ = serv_gets()
    format_type% = 0
    b$ = ""
    source_node$ = ""
    source_hnode$ = ""
    intext% = 0
    from_panel.Caption = ""
    to_panel.Caption = ""
    room_panel.Caption = ""
    date_panel.Caption = ""
    
    If Left$(a$, 1) = "1" Then
        message_text.Text = a$
        Do
            a$ = serv_gets()
            If a$ = "000" Then Exit Do
            If intext% = 0 Then
                If Left$(a$, 5) = "from=" Then from_panel.Caption = Right$(a$, Len(a$) - 5)
                If Left$(a$, 5) = "rcpt=" Then to_panel.Caption = Right$(a$, Len(a$) - 5)
                If Left$(a$, 5) = "room=" Then room_panel.Caption = Right$(a$, Len(a$) - 5)
                If Left$(a$, 5) = "time=" Then date_panel.Caption = strtime(Val(Right$(a$, Len(a$) - 5)))
                If Left$(a$, 5) = "type=" Then format_type% = Val(Right$(a$, Len(a$) - 5))
                If Left$(a$, 5) = "node=" Then source_node$ = Right$(a$, Len(a$) - 5)
                If Left$(a$, 5) = "hnod=" Then source_hnode$ = Right$(a$, Len(a$) - 5)
                If Left$(a$, 4) = "text" Then
                    intext% = 1
                    If format_type% = 1 Then
                        message_text.FontName = fixedfontname
                        message_text.FontSize = Int(message_text.Width / 1000)
                        message_text.FontBold = False
                        message_text.FontItalic = False
                    Else
                        message_text.FontName = varifontname
                        message_text.FontSize = varifontsize
                        message_text.FontBold = False
                        message_text.FontItalic = False
                        End If
                    End If
            Else
                b$ = b$ + a$ + Chr$(13) + Chr$(10)
                End If
            
            Loop
        If format_type% = 0 Then
            message_text.Text = Cit_Format(b$)
        Else
            message_text.Text = b$
            End If
        If source_node$ <> "" Then from_panel.Caption = from_panel.Caption + " @ " + source_node$
        If source_hnode$ <> "" Then from_panel.Caption = from_panel.Caption + " (" + source_hnode$ + ")"
    Else
        message_text.Text = a$
        End If
    
    Call end_trans
skipitall:
    Call message_text_Click

End Sub

Private Sub first_button_Click()
    curr_msg% = 0
    Call display_msg
End Sub

Private Sub Form_Load()

    varifontname = GetPrivateProfileVBString("Preferences", "VariFontName", "MS Sans Serif", "WINCIT.INI")
    varifontsize = GetPrivateProfileInt("Preferences", "VariFontSize", 10, "WINCIT.INI")

    fixedfontname = GetPrivateProfileVBString("Preferences", "FixedFontName", "Courier New", "WINCIT.INI")
    
    curr_msg% = 0

    move_button.Enabled = IsRoomAide%
    delete_button.Enabled = IsRoomAide%

    Show
    DoEvents
    Call display_msg

End Sub

Private Sub Form_Resize()
    
    ReadMessages.Left = 0
    ReadMessages.Top = 0
    ReadMessages.Width = MainWin.Width
    ReadMessages.Height = MainWin.Height - 400
    
    stop_button.Top = Abs(ReadMessages.Height - 520)
    last_button.Top = Abs(ReadMessages.Height - 520)
    next_button.Top = Abs(ReadMessages.Height - 520)
    back_button.Top = Abs(ReadMessages.Height - 520)
    first_button.Top = Abs(ReadMessages.Height - 520)
    move_button.Top = Abs(ReadMessages.Height - 520)
    delete_button.Top = Abs(ReadMessages.Height - 520)
    Quote_Button.Top = Abs(ReadMessages.Height - 520)
    print_button.Top = Abs(ReadMessages.Height - 520)
    msg_num.Top = Abs(ReadMessages.Height - 520)
    message_text.Width = Abs(ReadMessages.Width - 336)
    message_text.Height = Abs(ReadMessages.Height - 1664)
    message_panel.Height = Abs(ReadMessages.Height - 1424)
    message_panel.Width = Abs(ReadMessages.Width - 100)
End Sub

Private Sub last_button_Click()
    curr_msg% = max_msgs% - 1
    Call display_msg
End Sub

Private Sub message_text_Change()
    Call check_quote_button
End Sub

Private Sub message_text_Click()
    Call check_quote_button
End Sub

Private Sub next_button_Click()

    curr_msg% = curr_msg% + 1
    Call display_msg

End Sub

Private Sub print_button_Click()

    a$ = message_text.Text
    message_text.Text = quotify(a$)



End Sub

Private Sub quote_button_Click()

    a$ = message_text.SelText
    Clipboard.SetText quotify(a$)

End Sub

Private Function quotify(instring As String) As String

a$ = instring
b$ = ""

Do
    
CHKLN:
    If Len(a$) = 0 Then GoTo CHKEND

    If Len(a$) < 72 Then
        b$ = b$ + " >" + a$ + Chr$(13) + Chr$(10)
        a$ = ""
        End If

    c = InStr(a$, Chr$(13) + Chr$(10))
    If (c < 72) And (c <> 0) Then
        b$ = b$ + " >" + Left$(a$, c - 1) + Chr$(13) + Chr$(10)
        a$ = Right$(a$, (Len(a$) - c) - 1)
        End If

    c = InStr(a$, " ")
    If (c > 72) Or (c = 0) Then
        b$ = b$ + " >" + Left$(a$, 72) + Chr$(13) + Chr$(10)
        If Len(a$) > 0 Then a$ = Right$(a$, Len(a$) - 72)
        GoTo CHKLN
        End If

    d = 0
    c = 1
    Do
        e = InStr(c, a$, " ")
        If e > 0 Then
            If c < 72 Then d = e
            c = e + 1
            End If
        
        Loop Until e = 0 Or c >= 72
ELN:

    If d <> 0 Then
        b$ = b$ + " >" + Left$(a$, d) + Chr$(13) + Chr$(10)
        a$ = Right$(a$, (Len(a$) - d) + 1)
        GoTo CHKLN
        End If

    Loop Until Len(a$) = 0
CHKEND:
    quotify = b$

End Function

Private Sub stop_button_Click()

    Unload ReadMessages
    Load RoomPrompt

End Sub

