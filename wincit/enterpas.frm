VERSION 4.00
Begin VB.Form EnterPassword 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   2625
   ClientLeft      =   960
   ClientTop       =   2820
   ClientWidth     =   6345
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
   Height          =   3030
   Left            =   900
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   2625
   ScaleWidth      =   6345
   Top             =   2475
   Width           =   6465
   Begin VB.CommandButton CancelButton 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Cancel"
      Height          =   492
      Left            =   3840
      TabIndex        =   5
      Top             =   2040
      Width           =   1692
   End
   Begin VB.CommandButton SetPass 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Change Password"
      Height          =   492
      Left            =   840
      TabIndex        =   4
      Top             =   2040
      Width           =   1692
   End
   Begin VB.TextBox ConfPass 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "Wingdings"
         charset         =   2
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   492
      Left            =   240
      TabIndex        =   3
      Top             =   1320
      Width           =   5892
   End
   Begin VB.TextBox NewPass 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BorderStyle     =   0  'None
      BeginProperty Font 
         name            =   "Wingdings"
         charset         =   2
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   528
      Left            =   240
      TabIndex        =   2
      Top             =   360
      Width           =   5892
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   855
      Left            =   120
      TabIndex        =   1
      Top             =   120
      Width           =   6135
      _Version        =   65536
      _ExtentX        =   10821
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Enter new password:"
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   855
      Left            =   120
      TabIndex        =   0
      Top             =   1080
      Width           =   6135
      _Version        =   65536
      _ExtentX        =   10821
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Enter it again to verify:"
   End
End
Attribute VB_Name = "EnterPassword"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub CancelButton_Click()
    Unload EnterPassword
    Load RoomPrompt
End Sub

Private Sub ConfPass_Change()
    If NewPass.Text <> "" And NewPass.Text = confpass.Text Then
        SetPass.Enabled = True
    Else
        SetPass.Enabled = False
        End If

End Sub

Private Sub Form_Load()

    Show
    EnterPassword.WindowState = 0
    EnterPassword.Top = Int((MainWin.Height - EnterPassword.Height) / 3)
    EnterPassword.Left = Int((MainWin.Width - EnterPassword.Width) / 2)
    DoEvents
    SetPass.Enabled = False
    NewPass.Text = ""
    confpass.Text = ""
    
End Sub

Private Sub NewPass_Change()
    If NewPass.Text <> "" And NewPass.Text = confpass.Text Then
        SetPass.Enabled = True
    Else
        SetPass.Enabled = False
        End If
End Sub

Private Sub SetPass_Click()
    If begin_trans() = True Then
        serv_puts ("SETP " + NewPass.Text)
        buf$ = serv_gets()
        If Left$(buf$, 1) <> "2" Then
            MsgBox Right$(buf$, Len(buf$) - 4), 48
            End If
        Unload EnterPassword
        Load RoomPrompt
        Call end_trans
        End If
End Sub

