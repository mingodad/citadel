VERSION 4.00
Begin VB.Form Invite 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Invite a user"
   ClientHeight    =   3225
   ClientLeft      =   2430
   ClientTop       =   2940
   ClientWidth     =   4710
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
   Height          =   3630
   Left            =   2370
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3225
   ScaleWidth      =   4710
   Top             =   2595
   Width           =   4830
   Begin Threed.SSPanel Panel3D1 
      Height          =   615
      Left            =   120
      TabIndex        =   5
      Top             =   1560
      Width           =   4455
      _Version        =   65536
      _ExtentX        =   7858
      _ExtentY        =   1085
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelInner      =   1
      Begin VB.ComboBox UserName 
         Appearance      =   0  'Flat
         BackColor       =   &H00FFFFFF&
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   420
         Left            =   120
         TabIndex        =   6
         Top             =   120
         Width           =   4215
      End
   End
   Begin VB.CommandButton invite_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Invite this user"
      Height          =   735
      Left            =   120
      TabIndex        =   0
      Top             =   2400
      Width           =   2175
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   735
      Left            =   2400
      TabIndex        =   4
      Top             =   2400
      Width           =   2175
   End
   Begin Threed.SSPanel RoomName 
      Height          =   735
      Left            =   120
      TabIndex        =   3
      Top             =   360
      Width           =   4455
      _Version        =   65536
      _ExtentX        =   7858
      _ExtentY        =   1296
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelInner      =   1
   End
   Begin VB.Label Label2 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "User to invite:"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   120
      TabIndex        =   1
      Top             =   1320
      Width           =   4455
   End
   Begin VB.Label Label1 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Room Name:"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   120
      TabIndex        =   2
      Top             =   120
      Width           =   4455
   End
End
Attribute VB_Name = "Invite"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()

    Unload Invite
    Load RoomPrompt

End Sub

Private Sub Form_Load()
    Show
    Invite.WindowState = 0
    Invite.Top = Int((MainWin.Height - Invite.Height) / 3)
    Invite.Left = Int((MainWin.Width - Invite.Width) / 2)
    RoomName.Caption = CurrRoomName$
    DoEvents

If begin_trans() = True Then
    
    serv_puts ("LIST")
    a$ = serv_gets()
    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If (a$ <> "000") Then UserName.AddItem (extract(a$, 0))
            DoEvents
            Loop Until a$ = "000"
        End If
    Call end_trans
    End If

End Sub

Private Sub invite_button_Click()

If begin_trans() = True Then

    serv_puts ("INVT " + UserName.Text)
    a$ = serv_gets()
    Call end_trans

    If Left$(a$, 1) <> "2" Then
        MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
    Else
        Unload Invite
        Load RoomPrompt
        End If

    End If

End Sub

