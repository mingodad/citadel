VERSION 4.00
Begin VB.Form EditInfo 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   0  'None
   ClientHeight    =   6915
   ClientLeft      =   -420
   ClientTop       =   1950
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
   Left            =   -480
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6915
   ScaleWidth      =   9600
   Top             =   1605
   Width           =   9720
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
      Left            =   5520
      TabIndex        =   2
      Top             =   6480
      Width           =   1572
   End
   Begin Threed.SSPanel message_panel 
      Height          =   6375
      Left            =   0
      TabIndex        =   3
      Top             =   0
      Width           =   9615
      _Version        =   65536
      _ExtentX        =   16960
      _ExtentY        =   11245
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
         Height          =   6135
         Left            =   120
         MultiLine       =   -1  'True
         ScrollBars      =   2  'Vertical
         TabIndex        =   1
         Top             =   120
         Width           =   9375
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
      TabIndex        =   4
      Top             =   6480
      Width           =   1092
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
End
Attribute VB_Name = "EditInfo"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Unload EditInfo
    Load RoomPrompt
End Sub

Private Sub Form_Load()

    EditInfo.Caption = "Room Info for " + CurrRoomName$
    message_text.FontName = GetPrivateProfileVBString("Preferences", "VariFontName", "MS Serif", "WINCIT.INI")
    message_text.FontSize = GetPrivateProfileInt("Preferences", "VariFontSize", 12, "WINCIT.INI")

    Show
    DoEvents
    
    If begin_trans() = True Then
        serv_puts ("RINF")
        a$ = serv_gets()
        If Left$(a$, 1) = "1" Then
            b$ = ""
            Do
                c$ = serv_gets()
                If c$ <> "000" Then b$ = b$ + c$ + Chr$(13) + Chr$(10)
                Loop Until c$ = "000"
            message_text.Text = Cit_Format(b$)
            End If
        Call end_trans
        End If
    
    message_text.SetFocus

End Sub

Private Sub Form_Resize()
    EditInfo.Left = 0
    EditInfo.Top = 0
    EditInfo.Width = MainWin.Width
    EditInfo.Height = MainWin.Height - 450

    cancel_button.Top = Abs(EditInfo.Height - 520)
    save_button.Top = Abs(EditInfo.Height - 520)
    ShowFmt.Top = Abs(EditInfo.Height - 520)
    message_text.Width = Abs(EditInfo.Width - 336)
    message_text.Height = Abs(EditInfo.Height - 864)
    message_panel.Height = Abs(EditInfo.Height - 624)
    message_panel.Width = Abs(EditInfo.Width - 100)
End Sub

Private Sub hold_button_Click()
    HoldMessage$ = message_text.Text
    Unload EditInfo
    Load RoomPrompt
End Sub

Private Sub InsQuote_Click()

     message_text.SelText = Clipboard.GetText()
     Call ShowFmt_Click

End Sub

Private Sub save_button_Click()
    
    a$ = message_text.Text
    Do While Right$(a$, 2) = Chr$(13) + Chr$(10)
        a$ = Left$(a$, Len(a$) - 2)
        Loop
    message_text.Text = Cit_Format(a$)
    DoEvents

    If begin_trans() = True Then
        a$ = "EINF 1"
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
            Unload EditInfo
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

