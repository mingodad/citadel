VERSION 4.00
Begin VB.Form EnterMessage 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   0  'None
   ClientHeight    =   6915
   ClientLeft      =   1410
   ClientTop       =   2880
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
   Left            =   1350
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6915
   ScaleWidth      =   9600
   Top             =   2535
   Width           =   9720
   Begin VB.CommandButton hold_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Hold"
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
      Left            =   6000
      TabIndex        =   15
      Top             =   6480
      Width           =   1092
   End
   Begin VB.CommandButton ShowFmt 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Show &Formatted"
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
      Left            =   2640
      TabIndex        =   14
      Top             =   6480
      Width           =   1572
   End
   Begin Threed.SSPanel CurrPos 
      Height          =   375
      Left            =   120
      TabIndex        =   13
      Top             =   6480
      Width           =   1695
      _Version        =   65536
      _ExtentX        =   2990
      _ExtentY        =   661
      _StockProps     =   15
      Caption         =   "0 / 0"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin VB.CommandButton InsQuote 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Insert &Quote"
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
      Left            =   4320
      TabIndex        =   12
      Top             =   6480
      Width           =   1572
   End
   Begin Threed.SSPanel message_panel 
      Height          =   5535
      Left            =   0
      TabIndex        =   11
      Top             =   840
      Width           =   10455
      _Version        =   65536
      _ExtentX        =   18441
      _ExtentY        =   9763
      _StockProps     =   15
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
            weight          =   400
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         Height          =   5295
         Left            =   120
         MultiLine       =   -1  'True
         ScrollBars      =   2  'Vertical
         TabIndex        =   1
         Top             =   120
         Width           =   10215
      End
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
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
      Left            =   7200
      TabIndex        =   10
      Top             =   6480
      Width           =   1092
   End
   Begin Threed.SSPanel room_panel 
      Height          =   375
      Left            =   6000
      TabIndex        =   9
      Top             =   420
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   661
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.24
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
      TabIndex        =   6
      Top             =   15
      Width           =   3615
      _Version        =   65536
      _ExtentX        =   6376
      _ExtentY        =   661
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.24
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
      TabIndex        =   5
      Top             =   420
      Width           =   4935
      _Version        =   65536
      _ExtentX        =   8705
      _ExtentY        =   661
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.24
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   3
      BorderWidth     =   4
      BevelOuter      =   1
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
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
      Left            =   8400
      TabIndex        =   0
      Top             =   6480
      Width           =   1092
   End
   Begin Threed.SSPanel from_panel 
      Height          =   375
      Left            =   480
      TabIndex        =   2
      Top             =   15
      Width           =   4935
      _Version        =   65536
      _ExtentX        =   8705
      _ExtentY        =   661
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.24
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
      TabIndex        =   8
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
      TabIndex        =   7
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
      TabIndex        =   4
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
      TabIndex        =   3
      Top             =   120
      Width           =   495
   End
End
Attribute VB_Name = "EnterMessage"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    HoldMessage$ = ""
    Unload EnterMessage
    Load RoomPrompt
End Sub

Private Sub Form_Load()

    message_text.FontName = GetPrivateProfileVBString("Preferences", "VariFontName", "MS Sans Serif", "WINCIT.INI")
    message_text.FontSize = GetPrivateProfileInt("Preferences", "VariFontSize", 10, "WINCIT.INI")
    If Len(HoldMessage$) > 0 Then
        message_text.Text = Cit_Format(HoldMessage$)
        End If
    to_panel.Caption = recp$
    room_panel.Caption = CurrRoomName$

    Show
    DoEvents
    message_text.SetFocus

End Sub

Private Sub Form_Resize()
    
    EnterMessage.Left = 0
    EnterMessage.Top = 0
    EnterMessage.Width = MainWin.Width
    EnterMessage.Height = MainWin.Height - 450
    
    cancel_button.Top = Abs(EnterMessage.Height - 520)
    save_button.Top = Abs(EnterMessage.Height - 520)
    InsQuote.Top = Abs(EnterMessage.Height - 520)
    ShowFmt.Top = Abs(EnterMessage.Height - 520)
    hold_button.Top = Abs(EnterMessage.Height - 520)
    CurrPos.Top = Abs(EnterMessage.Height - 520)
    message_text.Width = Abs(EnterMessage.Width - 336)
    message_text.Height = Abs(EnterMessage.Height - 1664)
    message_panel.Height = Abs(EnterMessage.Height - 1424)
    message_panel.Width = Abs(EnterMessage.Width - 100)
End Sub

Private Sub hold_button_Click()
    HoldMessage$ = message_text.Text
    Unload EnterMessage
    Load RoomPrompt
End Sub

Private Sub InsQuote_Click()

     message_text.SelText = Clipboard.GetText()
     Call ShowFmt_Click

End Sub

Private Sub message_text_Click()
    
    CurrPos.Caption = Str$(message_text.SelStart) + " / " + Str$(Len(message_text.Text))

End Sub

Private Sub message_text_KeyDown(KeyCode As Integer, Shift As Integer)

    CurrPos.Caption = Str$(message_text.SelStart) + " / " + Str$(Len(message_text.Text))

End Sub

Private Sub save_button_Click()
    
    a$ = message_text.Text
    Do While Right$(a$, 2) = Chr$(13) + Chr$(10)
        a$ = Left$(a$, Len(a$) - 2)
        Loop
    message_text.Text = Cit_Format(a$)
    DoEvents

    If begin_trans() = True Then
        a$ = "ENT0 1|" + recp$
        serv_puts (a$)
        a$ = serv_gets()
        If Left$(a$, 1) <> "4" Then
            Call end_trans
            MsgBox a$
        Else
            Transmit_Buffer (message_text.Text)
            serv_puts ("000")
            Call end_trans
            HoldMessage$ = ""
            Unload EnterMessage
            Load RoomPrompt
            End If
        End If

End Sub

Private Sub ShowFmt_Click()
    a$ = message_text.Text
    Do While Right$(a$, 2) = Chr$(13) + Chr$(10)
        a$ = Left$(a$, Len(a$) - 2)
        Loop
    message_text.Text = Cit_Format(a$)

End Sub

