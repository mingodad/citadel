VERSION 4.00
Begin VB.Form EditRoom 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Edit this room"
   ClientHeight    =   6105
   ClientLeft      =   105
   ClientTop       =   2340
   ClientWidth     =   7410
   ClipControls    =   0   'False
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
   Height          =   6510
   Left            =   45
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6105
   ScaleWidth      =   7410
   Top             =   1995
   Width           =   7530
   Begin VB.CheckBox check_ReadOnly 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Read-only room"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   3840
      TabIndex        =   29
      Top             =   4920
      Width           =   3375
   End
   Begin VB.TextBox DirName 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   285
      Left            =   3840
      TabIndex        =   28
      Text            =   "Directory"
      Top             =   2880
      Width           =   3375
   End
   Begin Threed.SSFrame Frame3D9 
      Height          =   1215
      Left            =   3720
      TabIndex        =   2
      Top             =   4200
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   2143
      _StockProps     =   14
      Caption         =   "Other options"
      ForeColor       =   0
      Begin VB.CheckBox check_netroom 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Network shared room"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   3
         Top             =   360
         Width           =   3375
      End
   End
   Begin Threed.SSFrame Frame3D8 
      Height          =   1215
      Left            =   120
      TabIndex        =   24
      Top             =   4200
      Width           =   3495
      _Version        =   65536
      _ExtentX        =   6165
      _ExtentY        =   2143
      _StockProps     =   14
      Caption         =   "Anonymous messages"
      ForeColor       =   0
      Begin VB.OptionButton radio_anonoption 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Prompt users for anonymity"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   27
         Top             =   840
         Width           =   3255
      End
      Begin VB.OptionButton radio_anononly 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "All messages anonymous"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   26
         Top             =   600
         Width           =   3255
      End
      Begin VB.OptionButton radio_noAnon 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "No anonymous messages"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   25
         Top             =   360
         Width           =   3255
      End
   End
   Begin VB.TextBox RoomAide 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   285
      Left            =   3840
      TabIndex        =   23
      Text            =   "Room Aide"
      Top             =   3720
      Width           =   3375
   End
   Begin Threed.SSFrame Frame3D7 
      Height          =   615
      Left            =   3720
      TabIndex        =   22
      Top             =   3480
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Room Aide"
      ForeColor       =   0
   End
   Begin Threed.SSFrame Frame3D6 
      Height          =   615
      Left            =   3720
      TabIndex        =   21
      Top             =   2640
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Directory name"
      ForeColor       =   0
   End
   Begin Threed.SSFrame Frame3D5 
      Height          =   1455
      Left            =   120
      TabIndex        =   16
      Top             =   2640
      Width           =   3495
      _Version        =   65536
      _ExtentX        =   6165
      _ExtentY        =   2566
      _StockProps     =   14
      Caption         =   "Directory options"
      ForeColor       =   0
      Begin VB.CheckBox check_visdir 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Visible directory"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   20
         Top             =   1080
         Width           =   3255
      End
      Begin VB.CheckBox check_download 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Downloading allowed"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   19
         Top             =   840
         Width           =   3255
      End
      Begin VB.CheckBox check_uploading 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Uploading allowed"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   18
         Top             =   600
         Width           =   3255
      End
      Begin VB.CheckBox check_directory 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Enable as a directory room"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   17
         Top             =   360
         Width           =   3255
      End
   End
   Begin Threed.SSFrame Frame3D4 
      Height          =   975
      Left            =   3720
      TabIndex        =   13
      Top             =   1560
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   1720
      _StockProps     =   14
      Caption         =   "Access options"
      ForeColor       =   0
      Begin VB.CheckBox CurrForget 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Cause users to forget room"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   15
         Top             =   600
         Width           =   3375
      End
      Begin VB.CheckBox PrefOnly 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Preferred users only"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   14
         Top             =   360
         Width           =   3375
      End
   End
   Begin Threed.SSFrame PWframe 
      Height          =   615
      Left            =   3720
      TabIndex        =   11
      Top             =   840
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Password"
      ForeColor       =   0
      Begin VB.TextBox Password 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   700
            size            =   9.75
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   285
         Left            =   120
         TabIndex        =   12
         Text            =   "Password"
         Top             =   240
         Width           =   3375
      End
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   1695
      Left            =   120
      TabIndex        =   6
      Top             =   840
      Width           =   3495
      _Version        =   65536
      _ExtentX        =   6165
      _ExtentY        =   2990
      _StockProps     =   14
      Caption         =   "Access"
      ForeColor       =   0
      Begin VB.OptionButton radio_invonly 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Invitation only"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   10
         Top             =   1080
         Width           =   1935
      End
      Begin VB.OptionButton radio_passworded 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Accessible using a password"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   9
         Top             =   840
         Width           =   3255
      End
      Begin VB.OptionButton radio_guessname 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Accessible by guessing room name"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   8
         Top             =   600
         Width           =   3255
      End
      Begin VB.OptionButton radio_public 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Public"
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   7
         Top             =   360
         Width           =   1935
      End
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   492
      Left            =   6120
      TabIndex        =   0
      Top             =   5520
      Width           =   1212
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      Height          =   492
      Left            =   4800
      TabIndex        =   1
      Top             =   5520
      Width           =   1212
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   735
      Left            =   120
      TabIndex        =   4
      Top             =   0
      Width           =   7215
      _Version        =   65536
      _ExtentX        =   12726
      _ExtentY        =   1296
      _StockProps     =   14
      Caption         =   "Room Name"
      ForeColor       =   0
      Begin VB.TextBox RoomName 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   375
         Left            =   120
         TabIndex        =   5
         Text            =   "Room Name"
         Top             =   240
         Width           =   6975
      End
   End
End
Attribute VB_Name = "EditRoom"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim roomFlags&

Private Sub cancel_button_Click()
    Unload EditRoom
    Load RoomPrompt
End Sub

Private Sub check_directory_Click()
        If check_directory.Value = 1 Then
            check_uploading.Enabled = True
            check_download.Enabled = True
            check_visdir.Enabled = True
            DirName.Enabled = True
        Else
            check_uploading.Enabled = False
            check_download.Enabled = False
            check_visdir.Enabled = False
            DirName.Enabled = True
            End If

End Sub

Private Sub DirName_Change()

If Len(DirName.Text) > 14 Then DirName.Text = Left$(DirName.Text, 14)

End Sub

Private Sub Form_Load()
    Show
    EditRoom.WindowState = 0
    EditRoom.Top = Int((MainWin.Height - EditRoom.Height) / 3)
    EditRoom.Left = Int((MainWin.Width - EditRoom.Width) / 2)
    DoEvents

    a$ = "        "

    If begin_trans() = True Then
        serv_puts ("GETR")
        a$ = serv_gets()
        serv_puts ("GETA")
        b$ = serv_gets()
        Call end_trans

        If Left$(a$, 1) <> "2" Then GoTo no_access
        a$ = Right$(a$, Len(a$) - 4)
        RoomName.Text = extract$(a$, 0)
        Password.Text = extract$(a$, 1)
        DirName.Text = extract$(a$, 2)
        roomFlags& = Val(extract$(a$, 3))
        
        If Left$(b$, 1) = "2" Then
            b$ = Right$(b$, Len(b$) - 4)
            RoomAide.Text = extract$(b$, 0)
        Else
            RoomAide.Text = ""
            End If

        pr& = roomFlags& And 4      ' Private
        gn& = roomFlags& And 16     ' GuessName
        pw& = roomFlags& And 8      ' Passworded

        Radio_Public.Value = False
        Radio_Guessname.Value = False
        Radio_Passworded.Value = False
        Radio_InvOnly.Value = False

        If pr& = 0 Then Radio_Public.Value = True
        If pr& = 4 And gn& = 16 Then Radio_Guessname.Value = True
        If pr& = 4 And pw& = 8 Then
            Radio_Passworded.Value = True
            Password.Enabled = True
        Else
            Password.Enabled = False
            End If
        If pr& = 4 And gn& = 0 And pw& = 0 Then Radio_InvOnly.Value = True

        If roomFlags& And 4096 Then
            PrefOnly.Value = 1
        Else
            PrefOnly.Value = 0
            End If

        If roomFlags& And 2048 Then
            check_netroom.Value = 1
        Else
            check_netroom.Value = 0
            End If

        If roomFlags& And 32 Then
            check_directory.Value = 1
            check_uploading.Enabled = True
            check_download.Enabled = True
            check_visdir.Enabled = True
            DirName.Enabled = True
        Else
            check_directory.Value = 0
            check_uploading.Enabled = False
            check_download.Enabled = False
            check_visdir.Enabled = False
            DirName.Enabled = True
            End If
        check_uploading.Value = (roomFlags& And 64) / 64
        check_download.Value = (roomFlags& And 128) / 128
        check_visdir.Value = (roomFlags& And 256) / 256

        If roomFlags& And 512 Then Radio_AnonOnly.Value = True
        If roomFlags& And 1024 Then Radio_AnonOption.Value = True
        If (roomFlags& And (512 Or 1024)) = 0 Then radio_NoAnon.Value = True

        check_readonly.Value = (roomFlags& And 8192) / 8192

        save_button_enabled = True
        End If
        GoTo end_load_edit
no_access:
        save_button.Enabled = False
        MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
end_load_edit:

End Sub

Private Sub Password_Change()
If Len(Password.Text) > 9 Then Password.Text = Left$(Password.Text, 9)
End Sub

Private Sub radio_guessname_Click()
    If Radio_Public.Value = False Then
        CurrForget.Enabled = True
    Else
        CurrForget.Value = 0
        CurrForget.Enabled = False
        End If

End Sub

Private Sub radio_invonly_Click()
    If Radio_Public.Value = False Then
        CurrForget.Enabled = True
    Else
        CurrForget.Value = 0
        CurrForget.Enabled = False
        End If

End Sub

Private Sub radio_passworded_Click()
    
    pwframe.Enabled = Radio_Passworded.Value
    Password.Enabled = Radio_Passworded.Value
    If Radio_Public.Value = False Then
        CurrForget.Enabled = True
    Else
        CurrForget.Value = 0
        CurrForget.Enabled = False
        End If

End Sub

Private Sub radio_public_Click()

    If Radio_Public.Value = False Then
        CurrForget.Enabled = True
    Else
        CurrForget.Value = 0
        CurrForget.Enabled = False
        End If

End Sub

Private Sub RoomName_Change()

If Len(RoomName.Text) > 19 Then RoomName.Text = Left$(RoomName.Text, 19)

End Sub

Private Sub save_button_Click()

    unload_ok% = 0

    roomFlags& = roomFlags& Or 4 Or 8 Or 16
    If Radio_Public.Value = True Then roomFlags& = roomFlags& - 4 - 8 - 16
    If Radio_Guessname.Value = True Then roomFlags& = roomFlags& - 8
    If Radio_Passworded.Value = True Then roomFlags& = roomFlags& - 16
    If Radio_InvOnly.Value = True Then roomFlags& = roomFlags& - 8 - 16

    roomFlags& = (roomFlags& Or 32 Or 64 Or 128 Or 256) - 32 - 64 - 128 - 256
    roomFlags& = roomFlags& Or (check_directory.Value * 32)
    roomFlags& = roomFlags& Or (check_uploading.Value * 64)
    roomFlags& = roomFlags& Or (check_download.Value * 128)
    roomFlags& = roomFlags& Or (check_visdir.Value * 256)

    roomFlags& = (roomFlags& Or 512 Or 1024) - 512 - 1024
    If Radio_AnonOnly.Value = True Then roomFlags& = roomFlags& Or 512
    If Radio_AnonOption.Value = True Then roomFlags& = roomFlags& Or 1024

    roomFlags& = (roomFlags& Or 2048) - 2048
    roomFlags& = roomFlags& Or (check_netroom.Value * 2048)

    roomFlags& = (roomFlags& Or 4096) - 4096
    roomFlags& = roomFlags& Or (PrefOnly.Value * 4096)

    roomFlags& = (roomFlags& Or 8192) - 8192
    roomFlags& = roomFlags& Or (check_readonly.Value * 8192)

    If begin_trans() = True Then
        unload_ok% = 1
        rf$ = Str$(roomFlags&)
        rf$ = Right$(rf$, Len(rf$) - 1)
        cf$ = Str$(CurrForget.Value)
        cf$ = Right$(cf$, Len(cf$) - 1)
        a$ = "SETR " + RoomName.Text + "|" + Password.Text + "|" + DirName.Text + "|" + rf$ + "|" + cf$
        serv_puts (a$)
        a$ = serv_gets()
        
        serv_puts ("SETA " + RoomAide.Text)
        b$ = serv_gets()
        Call end_trans

        If Left$(a$, 1) <> "2" Then
            unload_ok% = 0
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
        Else
            CurrRoomName$ = RoomName.Text
            End If

        If Left$(b$, 1) <> "2" Then
            unload_ok% = 0
            MsgBox Right$(b$, Len(b$) - 4), 16, "Error"
            End If
        End If

    If unload_ok% = 1 Then
        Unload EditRoom
        Load RoomPrompt
        End If

End Sub

