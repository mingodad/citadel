VERSION 4.00
Begin VB.Form Recipient 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   1530
   ClientLeft      =   870
   ClientTop       =   1530
   ClientWidth     =   6420
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
   Height          =   1935
   Left            =   810
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   1530
   ScaleWidth      =   6420
   Top             =   1185
   Width           =   6540
   Begin Threed.SSPanel StatMsg 
      Height          =   495
      Left            =   120
      TabIndex        =   4
      Top             =   960
      Width           =   3735
      _Version        =   65536
      _ExtentX        =   6588
      _ExtentY        =   873
      _StockProps     =   15
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin VB.CommandButton ok_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&OK"
      Height          =   492
      Left            =   3960
      TabIndex        =   3
      Top             =   960
      Width           =   1092
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   492
      Left            =   5160
      TabIndex        =   2
      Top             =   960
      Width           =   1212
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   855
      Left            =   120
      TabIndex        =   1
      Top             =   0
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Enter recipient:"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Begin VB.TextBox recpname 
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
         Height          =   372
         Left            =   120
         TabIndex        =   0
         Top             =   360
         Width           =   6012
      End
   End
End
Attribute VB_Name = "Recipient"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Unload Recipient
    Load RoomPrompt
End Sub

Private Sub Form_Load()

    Recipient.WindowState = 0
    Recipient.Top = Abs((MainWin.Height - Recipient.Height) / 3)
    Recipient.Left = Abs((MainWin.Left - Recipient.Width) / 2)
    Show

End Sub

Private Sub ok_button_Click()

    If begin_trans() = True Then
        serv_puts ("ENT0 0|" + recpname.Text)
        a$ = serv_gets()
        Call end_trans
        If Left$(a$, 1) = "2" Then
            recp$ = recpname.Text
            Unload Recipient
            Load EnterMessage
        Else
            statmsg.Caption = Right$(a$, Len(a$) - 4)
            End If
        End If

End Sub

Private Sub recpname_KeyPress(keyascii As Integer)
    If keyascii = 13 Then Call ok_button_Click
    If keyascii = 27 Then Call cancel_button_Click
End Sub

