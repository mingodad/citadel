VERSION 4.00
Begin VB.Form EnterRoom 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Enter a new room..."
   ClientHeight    =   3840
   ClientLeft      =   1980
   ClientTop       =   3390
   ClientWidth     =   5640
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
   Height          =   4245
   Left            =   1920
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3840
   ScaleWidth      =   5640
   Top             =   3045
   Width           =   5760
   Begin VB.OptionButton NewRoomType 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Invitation-only (explicit invitation by Aide or Room Aide)"
      ForeColor       =   &H80000008&
      Height          =   255
      Index           =   3
      Left            =   240
      TabIndex        =   10
      Top             =   1920
      Width           =   5175
   End
   Begin VB.OptionButton NewRoomType 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Passworded (hidden plus password required for access)"
      ForeColor       =   &H80000008&
      Height          =   255
      Index           =   2
      Left            =   240
      TabIndex        =   9
      Top             =   1680
      Width           =   5175
   End
   Begin VB.OptionButton NewRoomType 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "GuessName (hidden until user explicitly types room name)"
      ForeColor       =   &H80000008&
      Height          =   255
      Index           =   1
      Left            =   240
      TabIndex        =   8
      Top             =   1440
      Width           =   5175
   End
   Begin VB.OptionButton NewRoomType 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Public (all users can access this room by default)"
      ForeColor       =   &H80000008&
      Height          =   255
      Index           =   0
      Left            =   240
      TabIndex        =   7
      Top             =   1200
      Width           =   5175
   End
   Begin VB.TextBox NewRoomPass 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   240
      TabIndex        =   6
      Top             =   2640
      Width           =   5175
   End
   Begin Threed.SSFrame Frame3D3 
      Height          =   735
      Left            =   120
      TabIndex        =   5
      Top             =   2400
      Width           =   5415
      _Version        =   65536
      _ExtentX        =   9551
      _ExtentY        =   1296
      _StockProps     =   14
      Caption         =   "Room Password"
      ForeColor       =   0
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   1455
      Left            =   120
      TabIndex        =   4
      Top             =   840
      Width           =   5415
      _Version        =   65536
      _ExtentX        =   9551
      _ExtentY        =   2566
      _StockProps     =   14
      Caption         =   "Type of room"
      ForeColor       =   0
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   735
      Left            =   120
      TabIndex        =   2
      Top             =   0
      Width           =   5415
      _Version        =   65536
      _ExtentX        =   9551
      _ExtentY        =   1296
      _StockProps     =   14
      Caption         =   "Name for new room"
      ForeColor       =   0
      Begin VB.TextBox NewRoomName 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   13.5
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   375
         Left            =   120
         TabIndex        =   3
         Top             =   240
         Width           =   5175
      End
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   492
      Left            =   4200
      TabIndex        =   1
      Top             =   3240
      Width           =   1332
   End
   Begin VB.CommandButton ok_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&OK (create)"
      Height          =   492
      Left            =   2760
      TabIndex        =   0
      Top             =   3240
      Width           =   1332
   End
End
Attribute VB_Name = "EnterRoom"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    
    Unload EnterRoom
    Load RoomPrompt

End Sub

Private Sub Form_Load()


    Show
    EnterRoom.WindowState = 0
    EnterRoom.Top = Int((MainWin.Height - EnterRoom.Height) / 3)
    EnterRoom.Left = Int((MainWin.Width - EnterRoom.Width) / 2)
    
    NewRoomType(0).Value = True
    NewRoomType(1).Value = False
    NewRoomType(2).Value = False
    NewRoomType(3).Value = False
    NewRoomPass.Enabled = NewRoomType(2).Value
    ok_button.Enabled = False

End Sub

Private Sub NewRoomName_Change()

    If Len(NewRoomName.Text) > 0 Then
        ok_button.Enabled = True
    Else
        ok_button.Enabled = False
        End If

End Sub

Private Sub NewRoomType_Click(Index As Integer)

    NewRoomPass.Enabled = NewRoomType(2).Value

End Sub

Private Sub ok_button_Click()

    b% = 0
    For c% = 0 To 3
        If NewRoomType(c%).Value = True Then b% = c%
        Next c%

    If begin_trans() = True Then
        serv_puts ("CRE8 1|" + NewRoomName.Text + "|" + Str$(b%) + "|" + NewRoomPass.Text)
        a$ = serv_gets()
        Call end_trans
        End If

    If Left$(a$, 1) <> "2" Then
        MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
    Else
        CurrRoomName$ = NewRoomName.Text
        SaveOldCount = 0
        SaveNewCount = 0
        Unload EnterRoom
        Load RoomPrompt
        End If

End Sub

