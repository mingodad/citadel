VERSION 4.00
Begin VB.Form RoomPrompt 
   AutoRedraw      =   -1  'True
   BorderStyle     =   0  'None
   ClientHeight    =   6210
   ClientLeft      =   1005
   ClientTop       =   1935
   ClientWidth     =   9600
   ForeColor       =   &H80000008&
   Height          =   6900
   Left            =   945
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6210
   ScaleWidth      =   9600
   ShowInTaskbar   =   0   'False
   Top             =   1305
   Width           =   9720
   Begin VB.CommandButton button_abandon 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Abandon"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Index           =   0
      Left            =   0
      TabIndex        =   21
      Top             =   720
      Width           =   1572
   End
   Begin VB.ListBox OldRooms 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Columns         =   6
      Height          =   1980
      Left            =   120
      Sorted          =   -1  'True
      TabIndex        =   17
      Top             =   3960
      Width           =   10095
   End
   Begin VB.ListBox NewRooms 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Columns         =   6
      Height          =   1980
      Left            =   120
      Sorted          =   -1  'True
      TabIndex        =   14
      Top             =   1560
      Width           =   10095
   End
   Begin VB.CommandButton button_ungoto 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Ungoto"
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
      Height          =   372
      Left            =   1560
      TabIndex        =   11
      Top             =   0
      Width           =   1572
   End
   Begin VB.CommandButton button_page 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Page a user"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Left            =   6240
      TabIndex        =   13
      Top             =   360
      Width           =   1572
   End
   Begin VB.CommandButton button_dir 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Read &Directory"
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
      Height          =   372
      Left            =   1560
      TabIndex        =   12
      Top             =   720
      Width           =   1572
   End
   Begin VB.CommandButton button_chat 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Chat"
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
      Left            =   4680
      TabIndex        =   5
      Top             =   720
      Width           =   1575
   End
   Begin VB.CommandButton button_who 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Who is online"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Left            =   6240
      TabIndex        =   6
      Top             =   0
      Width           =   1572
   End
   Begin VB.CommandButton button_terminate 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Terminate"
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
      Index           =   3
      Left            =   6240
      TabIndex        =   2
      Top             =   720
      Width           =   1575
   End
   Begin VB.CommandButton button_skip 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Skip"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Index           =   1
      Left            =   0
      TabIndex        =   1
      Top             =   360
      Width           =   1572
   End
   Begin VB.CommandButton button_zap 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Zap (forget)"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Left            =   1560
      TabIndex        =   7
      Top             =   360
      Width           =   1572
   End
   Begin VB.CommandButton button_forward 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Read &Forward"
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
      Index           =   5
      Left            =   4680
      TabIndex        =   4
      Top             =   0
      Width           =   1575
   End
   Begin VB.CommandButton button_old 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Read &Old"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Left            =   3120
      TabIndex        =   10
      Top             =   360
      Width           =   1572
   End
   Begin VB.CommandButton button_new 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Read &New"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Left            =   3120
      TabIndex        =   9
      Top             =   0
      Width           =   1572
   End
   Begin VB.CommandButton button_last5 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Read &Last 5"
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
      Left            =   4680
      TabIndex        =   8
      Top             =   360
      Width           =   1575
   End
   Begin VB.CommandButton button_goto 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Goto"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Index           =   0
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   1572
   End
   Begin VB.CommandButton button_enter 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Enter message"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   372
      Index           =   4
      Left            =   3120
      TabIndex        =   3
      Top             =   720
      Width           =   1572
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   1335
      Left            =   7830
      TabIndex        =   18
      Top             =   0
      Width           =   1755
      _Version        =   65536
      _ExtentX        =   3096
      _ExtentY        =   2355
      _StockProps     =   14
      Caption         =   "Current room:"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Begin VB.Label MsgsCount 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00808080&
         BackStyle       =   0  'Transparent
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
         Height          =   372
         Left            =   36
         TabIndex        =   20
         Top             =   840
         Width           =   456
      End
      Begin VB.Label CurrRoom 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         AutoSize        =   -1  'True
         BackColor       =   &H00808080&
         BackStyle       =   0  'Transparent
         BeginProperty Font 
            name            =   "Arial"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H00000000&
         Height          =   285
         Left            =   225
         TabIndex        =   19
         Top             =   360
         Width           =   60
      End
   End
   Begin VB.Label Label2 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H00C0C0C0&
      Caption         =   "No unseen messages in:"
      ForeColor       =   &H00800000&
      Height          =   240
      Left            =   120
      TabIndex        =   16
      Top             =   3720
      Width           =   2184
   End
   Begin VB.Label Label1 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H00C0C0C0&
      Caption         =   "Rooms with unread messages:"
      ForeColor       =   &H00800000&
      Height          =   240
      Left            =   120
      TabIndex        =   15
      Top             =   1200
      Width           =   2736
   End
   Begin VB.Menu menu_enter 
      Caption         =   "&Enter"
      Begin VB.Menu menu_enter_conf 
         Caption         =   "&Configuration"
      End
      Begin VB.Menu menu_enter_file 
         Caption         =   "&File (upload)"
      End
      Begin VB.Menu menu_enter_regis 
         Caption         =   "Re&gistration Info"
      End
      Begin VB.Menu menu_enter_passwd 
         Caption         =   "Set &Password..."
      End
      Begin VB.Menu menu_enter_room 
         Caption         =   "Create New &Room..."
      End
   End
   Begin VB.Menu menu_read 
      Caption         =   "&Read"
      Begin VB.Menu menu_read_directory 
         Caption         =   "&Directory"
      End
      Begin VB.Menu menu_read_file 
         Caption         =   "&File (download)"
      End
      Begin VB.Menu menu_read_info 
         Caption         =   "Room &Info File"
      End
      Begin VB.Menu menu_read_userlist 
         Caption         =   "&User List"
      End
   End
   Begin VB.Menu menu_term 
      Caption         =   "&Terminate"
      Begin VB.Menu menu_term_quit 
         Caption         =   "Terminate and &Quit"
      End
      Begin VB.Menu menu_term_stay 
         Caption         =   "Terminate and &Stay Online"
      End
   End
   Begin VB.Menu menu_aide 
      Caption         =   "&Aide"
      Begin VB.Menu menu_aide_edit 
         Caption         =   "&Edit Room..."
      End
      Begin VB.Menu menu_aide_file 
         Caption         =   "&File Commands..."
         Begin VB.Menu menu_aide_file_delete 
            Caption         =   "&Delete File..."
         End
         Begin VB.Menu menu_aide_file_move 
            Caption         =   "&Move File..."
         End
         Begin VB.Menu menu_aide_file_send 
            Caption         =   "&Send File over Net..."
         End
      End
      Begin VB.Menu menu_aide_info 
         Caption         =   "Edit &Info File"
      End
      Begin VB.Menu menu_aide_kill 
         Caption         =   "&Kill this Room"
      End
      Begin VB.Menu menu_aide_room 
         Caption         =   "&Room commands..."
         Begin VB.Menu menu_aide_room_invite 
            Caption         =   "&Invite a user..."
         End
         Begin VB.Menu menu_aide_room_kick 
            Caption         =   "&Kick out a user..."
         End
      End
      Begin VB.Menu menu_aide_useredit 
         Caption         =   "Edit a &User..."
      End
      Begin VB.Menu menu_aide_valid 
         Caption         =   "&Validate New Users..."
      End
      Begin VB.Menu menu_aide_whoknows 
         Caption         =   "&Who Knows Room"
      End
   End
End
Attribute VB_Name = "RoomPrompt"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim newmsgs%

Private Sub button_abandon_Click(Index As Integer)

    If begin_trans() = True Then
        serv_puts ("SLRP " + Str$(LastMessageRead&))
        a$ = serv_gets()
        Call end_trans
        newmsgs% = 1
        Call button_skip_click(0)
        End If

End Sub

Private Sub button_chat_Click()

If begin_trans() = True Then
    serv_puts ("CHAT")
    a$ = serv_gets()
    If Left$(a$, 1) = "8" Then
        Unload RoomPrompt
        Load ChatWindow
    Else
        end_trans
        End If
    End If

End Sub

Private Sub button_dir_Click()
    Call menu_read_directory_click
End Sub

Private Sub button_enter_Click(Index As Integer)

    recp$ = ""
    can_enter% = 0

    If begin_trans() = True Then
              serv_puts ("ENT0")
              a$ = serv_gets()
              If Left$(a$, 1) = "2" Then can_enter% = 1
              If Left$(a$, 3) = "570" Then can_enter% = 2
        Call end_trans
        End If

    If can_enter% = 0 Then
        MsgBox Right$(a$, Len(a$) - 4)
        End If
    If can_enter% = 1 Then
        Unload RoomPrompt
        Load EnterMessage
        End If
    If can_enter% = 2 Then
        Unload RoomPrompt
        Load Recipient
        End If

End Sub

Private Sub button_forward_Click(Index As Integer)

    max_msgs% = 0
    If begin_trans() = False Then GoTo skipfwd
    serv_puts ("MSGS ALL")
    a$ = serv_gets()

    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If a$ = "000" Then Exit Do
            msg_array&(max_msgs%) = CLng(Val(a$))
            max_msgs% = max_msgs% + 1
            Loop
        End If
    Call end_trans
skipfwd:

    If max_msgs% > 0 Then
        Unload RoomPrompt
        Load ReadMessages
    Else
        MsgBox "This room is empty."
        End If

End Sub

Private Sub button_goto_click(Index As Integer)

    If begin_trans() = True Then
        serv_puts ("SLRP HIGHEST")
        a$ = serv_gets()
        Call end_trans
        If NewRooms.ListIndex >= 0 Then newmsgs% = 0
        If OldRooms.ListIndex >= 0 Then newmsgs% = 1
        Call button_skip_click(0)
        End If

End Sub


Private Sub button_last5_Click()
    max_msgs% = 0
    If begin_trans() = False Then GoTo skiplast5
    serv_puts ("MSGS LAST|5")
    a$ = serv_gets()

    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If a$ = "000" Then Exit Do
            msg_array&(max_msgs%) = CLng(Val(a$))
            max_msgs% = max_msgs% + 1
            Loop
        End If
    Call end_trans
skiplast5:

    If max_msgs% > 0 Then
        Unload RoomPrompt
        Load ReadMessages
    Else
        MsgBox "This room is empty."
        End If



End Sub

Private Sub button_new_Click()
    max_msgs% = 0
    If begin_trans() = False Then GoTo skipnew
    serv_puts ("MSGS NEW")
    a$ = serv_gets()

    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If a$ = "000" Then Exit Do
            msg_array&(max_msgs%) = CLng(Val(a$))
            max_msgs% = max_msgs% + 1
            Loop
        End If
    Call end_trans
skipnew:

    If max_msgs% > 0 Then
        Unload RoomPrompt
        Load ReadMessages
    Else
        MsgBox "This room is empty."
        End If


End Sub

Private Sub button_old_Click()
    max_msgs% = 0
    If begin_trans() = False Then GoTo skipold
    serv_puts ("MSGS OLD")
    a$ = serv_gets()

    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If a$ = "000" Then Exit Do
            msg_array&(max_msgs%) = CLng(Val(a$))
            max_msgs% = max_msgs% + 1
            Loop
        End If
    Call end_trans
skipold:

    If max_msgs% > 0 Then
        Unload RoomPrompt
        Load ReadMessages
    Else
        MsgBox "This room is empty."
        End If


End Sub

Private Sub button_page_Click()

Unload RoomPrompt
Load PageUser

End Sub

Private Sub button_skip_click(Index As Integer)

    h$ = CurrRoomName$
    n% = newmsgs%
    
    a% = OldRooms.ListIndex
    If (a% >= 0) Then
        b$ = OldRooms.List(a%)
        End If

    a% = NewRooms.ListIndex
    If (a% >= 0) Then
        b$ = NewRooms.List(a%)
        End If

    Call goto_room(b$)
    If n% = 0 Then
        OldRooms.AddItem (h$)
    Else
        NewRooms.AddItem (h$)
        End If
    
End Sub

Private Sub button_terminate_click(Index As Integer)

    If begin_trans() = True Then
        serv_puts ("LOUT")
        buf$ = serv_gets()
        Call end_trans
        Unload RoomPrompt
        Load CitUser
        End If

End Sub

Private Sub button_who_Click()
    Unload RoomPrompt
    Load WhoIsOnline
End Sub

Private Sub button_zap_Click()
    q% = MsgBox("Are you sure you want to Zap (forget) '" + CurrRoomName$ + "'?", 33, CurrRoomName$)

    If q% = 1 Then

        If begin_trans() = True Then
    
            serv_puts ("FORG")
            a$ = serv_gets()
            Call end_trans
            End If

        If Left$(a$, 1) <> "2" Then
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
        Else
            Call goto_room("_BASEROOM_")
            End If
        End If

End Sub

Private Sub Form_Load()
    NewRooms.Left = RoomPrompt.Left + 120
    NewRooms.Width = RoomPrompt.Width - 240

    OldRooms.Left = RoomPrompt.Left + 120
    OldRooms.Width = RoomPrompt.Width - 240
    
    Show
    RoomPrompt.WindowState = 2
    
    If SaveOldCount = 0 And SaveNewCount = 0 Then
        Call RefreshRoomLists
    Else
        NewRooms.Clear
        OldRooms.Clear
        For a% = 1 To SaveNewCount
            NewRooms.AddItem (SaveNewRooms$(a%))
            Next a%
        For a% = 1 To SaveOldCount
            OldRooms.AddItem (SaveOldRooms$(a%))
            Next a%
        End If
    If CurrRoomName$ = "" Then CurrRoomName$ = "_BASEROOM_"
    Call goto_room(CurrRoomName$)

    If LastMessageRead& > 0 Then
        button_abandon(0).Enabled = True
    Else
        button_abandon(0).Enabled = False
        End If

    Show

End Sub

Private Sub Form_Resize()

    RoomPrompt.Left = 0
    RoomPrompt.Top = 0
    RoomPrompt.Width = MainWin.Width - 200
    RoomPrompt.Height = MainWin.Height - 600

    NewRooms.Left = RoomPrompt.Left + 60
    NewRooms.Width = RoomPrompt.Width - 120

    OldRooms.Left = RoomPrompt.Left + 60
    OldRooms.Width = RoomPrompt.Width - 120

    NewRooms.Top = Label1.Top + Label1.Height
    NewRooms.Height = Abs(Int(((RoomPrompt.Height - Label1.Top) / 2) - 300))

    Label2.Top = NewRooms.Top + NewRooms.Height + 20
    OldRooms.Top = Label2.Top + Label2.Height
    OldRooms.Height = Abs((RoomPrompt.Height - OldRooms.Top) - 300)

    Frame3D1.Width = Abs((RoomPrompt.Width - Frame3D1.Left) - 120)
    CurrRoom.Width = Abs(Frame3D1.Width - 150)
    MsgsCount.Width = Abs(Frame3D1.Width - 150)

    'CurrRoom.FontSize = Abs(Frame3D1.Width / 225)

End Sub

Private Sub Form_Unload(Cancel As Integer)

    ' Save the room lists
    SaveNewCount = NewRooms.ListCount
    For a% = 1 To SaveNewCount
        SaveNewRooms$(a%) = NewRooms.List(a% - 1)
        Next a%
    SaveOldCount = OldRooms.ListCount
    For a% = 1 To SaveOldCount
        SaveOldRooms$(a%) = OldRooms.List(a% - 1)
        Next a%


End Sub

Private Sub goto_room(RoomName As String)

    If begin_trans() = False Then GoTo skipgoto
    If RoomName <> CurrRoomName$ Then LastMessageRead& = 0
    serv_puts ("GOTO " + RoomName)
    a$ = serv_gets()
    Call end_trans

    If (Left$(a$, 1) <> "2") Then
        serv_puts ("GOTO" + "_BASEROOM_")
        a$ = serv_gets()
        End If

    a$ = Right$(a$, Len(a$) - 4)
    CurrRoomName$ = extract$(a$, 0)
    CurrRoomFlags% = Val(extract$(a$, 4))
    RoomPrompt.Caption = CurrRoomName$
    button_abandon(0).Enabled = False

    ' Enable file transfer commands only if we're in a directory room
    If (CurrRoomFlags% And 32) = 32 Then
        b = True
    Else
        b = False
        End If

    ' Enable some aide commands if user is the room aide
    IsRoomAide% = False
    If Val(extract$(a$, 8)) > 0 Then IsRoomAide% = True

    menu_read_directory.Enabled = b
    button_dir.Enabled = b
    menu_aide_file.Enabled = (b And IsRoomAide%)
    menu_read_file.Enabled = b
    menu_enter_file.Enabled = b
    menu_aide_edit.Enabled = IsRoomAide%
    menu_aide_info.Enabled = IsRoomAide%
    menu_aide_kill.Enabled = IsRoomAide%
    menu_aide_whoknows.Enabled = IsRoomAide%

    If (axlevel >= 6) Or (IsRoomAide% = True) Then
        menu_aide.Enabled = True
    Else
        menu_aide.Enabled = False
        End If

    CurrRoom.Caption = CurrRoomName$
    newmsgs% = Val(extract$(a$, 1))
    MsgsCount.Caption = extract$(a$, 2) + " messages" + Chr$(13) + Chr$(10) + extract(a$, 1) + " new"

    If (NewRooms.ListCount >= 0) Then
        For c% = 0 To NewRooms.ListCount - 1
            If NewRooms.List(c%) = CurrRoomName$ Then NewRooms.RemoveItem (c%)
            Next
        End If

    If (OldRooms.ListCount >= 0) Then
        For c% = 0 To OldRooms.ListCount - 1
            If OldRooms.List(c%) = CurrRoomName$ Then OldRooms.RemoveItem (c%)
            Next
        End If

    If (OldRooms.ListIndex = (-1)) And (NewRooms.ListIndex = (-1)) And NewRooms.ListCount > 0 Then
        NewRooms.ListIndex = 0
    Else
        If (OldRooms.ListIndex = (-1)) And (NewRooms.ListIndex = (-1)) And OldRooms.ListCount > 0 Then
            OldRooms.ListIndex = 0
            End If
        End If

   If Val(extract$(a$, 3)) > 0 Then Call ReadRoomInfo(0)

skipgoto:

End Sub

Private Sub menu_aide_edit_Click()

    Unload RoomPrompt
    Load EditRoom

End Sub

Private Sub menu_aide_info_Click()

    If begin_trans() = True Then
        serv_puts ("EINF 0")
        a$ = serv_gets()
        Call end_trans
        If Left$(a$, 1) = "2" Then
            Unload RoomPrompt
            Load EditInfo
        Else
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
            End If
        End If

End Sub

Private Sub menu_aide_kill_Click()

    q% = 2

    If begin_trans() = True Then
        serv_puts ("KILL 0")
        a$ = serv_gets()
        Call end_trans
        End If

    If Left$(a$, 1) = "2" Then
        q% = MsgBox("Are you sure you want to delete '" + CurrRoomName$ + "'?", 33, CurrRoomName$)
    Else
        MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
        End If

    If q% = 1 Then

        If begin_trans() = True Then
    
            serv_puts ("KILL 1")
            a$ = serv_gets()
            Call end_trans
            End If

        If Left$(a$, 1) <> "2" Then
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
        Else
            Call goto_room("_BASEROOM_")
            End If
        End If

End Sub

Private Sub menu_aide_room_invite_Click()

    Unload RoomPrompt
    Load Invite

End Sub

Private Sub menu_aide_room_kick_Click()

    Unload RoomPrompt
    Load KickOut

End Sub

Private Sub menu_aide_valid_Click()

    Unload RoomPrompt
    Load Validation

End Sub

Private Sub menu_aide_whoknows_Click()

    Unload RoomPrompt
    Load WhoKnowsRoom

End Sub

Private Sub menu_enter_conf_Click()
    Unload RoomPrompt
    Load EnterConfiguration
End Sub

Private Sub menu_enter_passwd_Click()
    Unload RoomPrompt
    Load EnterPassword
End Sub

Private Sub menu_enter_regis_Click()
    Unload RoomPrompt
    Load Registration
End Sub

Private Sub menu_enter_room_Click()

    If begin_trans() = True Then
        serv_puts ("CRE8 0|0|0|0")
        a$ = serv_gets()
        Call end_trans
        If Left$(a$, 1) = "2" Then
            Unload RoomPrompt
            Load EnterRoom
        Else
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
            End If
        End If

End Sub

Private Sub menu_help_about_Click()

n$ = Chr$(13) + Chr$(10)
a$ = ""
a$ = "Citadel/UX Client for Windows" + n$
a$ = a$ + "Copyright © 1995 by Art Cancro" + n$
a$ = a$ + "WinSock code Copyright © 1994 by Brian Syme" + n$
a$ = a$ + n$
a$ = a$ + "The home of Citadel/UX is UNCENSORED! BBS at 914-244-3252." + n$
a$ = a$ + n$
a$ = a$ + "THIS IS A PRELIMINARY AND UNFINISHED DISTRIBUTION" + n$
a$ = a$ + "FOR DEMONSTRATION PURPOSES ONLY."

MsgBox a$
End Sub

Private Sub menu_read_directory_click()

    Unload RoomPrompt
    Load RoomDirectory


End Sub

Private Sub menu_read_file_Click()

    Unload RoomPrompt
    Load Download

End Sub

Private Sub menu_read_info_click()

Call ReadRoomInfo(1)
End Sub
Private Sub ReadRoomInfo(deliberate As Integer)

foundit = 0
    If begin_trans() = True Then
        serv_puts ("RINF")
        a$ = serv_gets()
        If Left$(a$, 1) = "1" Then
        foundit = 1
        i$ = ""
        Do
            a$ = serv_gets()
            If a$ <> "000" Then i$ = i$ + a$ + Chr$(13) + Chr$(10)
            Loop While a$ <> "000"
        Else
            i$ = "No info file available for this room."
            End If
        Call end_trans
        If foundit = 1 Or deliberate = 1 Then
            MsgBox Cit_Format(i$), 64, CurrRoomName$
            End If
        End If

End Sub

Private Sub menu_read_userlist_Click()
    Unload RoomPrompt
    Load UserList
End Sub

Private Sub menu_term_quit_Click()
    
  
        If begin_trans() = True Then
         serv_puts ("QUIT")
         buf$ = serv_gets()
         Call end_trans
         End If

    Unload RoomPrompt
    Unload IPC
    Load SelectBBS
        
End Sub

Private Sub menu_term_stay_Click()
    Call button_terminate_click(0)
End Sub

Private Sub NewRooms_Click()

    a% = NewRooms.ListIndex
    OldRooms.ListIndex = (-1)
    NewRooms.ListIndex = a%
    
End Sub

Private Sub NewRooms_DblClick()

    If DoubleClickAction$ = "GOTO" Then Call button_goto_click(0)
    If DoubleClickAction$ = "SKIP" Then Call button_skip_click(0)
    If DoubleClickAction$ = "ABANDON" Then Call button_abandon_Click(0)


End Sub

Private Sub NewRooms_KeyPress(keyascii As Integer)

    If keyascii = 13 Or keyascii = 10 Then Call NewRooms_DblClick

End Sub

Private Sub OldRooms_Click()
    
    a% = OldRooms.ListIndex
    NewRooms.ListIndex = (-1)
    OldRooms.ListIndex = a%

End Sub

Private Sub OldRooms_DblClick()
    
    Call button_skip_click(0)

End Sub

Private Sub OldRooms_KeyPress(keyascii As Integer)

    If keyascii = 13 Or keyascii = 10 Then Call OldRooms_DblClick

End Sub

Private Sub RefreshRoomLists()

    ' First, clear out the saved rooms arrays...
    SaveNewCount = 0
    SaveOldCount = 0

    ' Then, clear out the windows if there's anything there...
    OldRooms.Clear
    NewRooms.Clear

    If begin_trans() = False Then GoTo skiproom
    serv_puts ("LKRN")
    buf$ = serv_gets()
    If Left$(buf$, 1) = "1" Then
        Do
            buf$ = serv_gets()
            If buf$ = "000" Then Exit Do
            NewRooms.AddItem (extract$(buf$, 0))
            Loop
        End If

    serv_puts ("LKRO")
    buf$ = serv_gets()
    If Left$(buf$, 1) = "1" Then
        Do
            buf$ = serv_gets()
            If buf$ = "000" Then Exit Do
            OldRooms.AddItem (extract$(buf$, 0))
            Loop
        End If
    Call end_trans
skiproom:

End Sub

