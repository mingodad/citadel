VERSION 4.00
Begin VB.Form Registration 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Registration"
   ClientHeight    =   3660
   ClientLeft      =   2820
   ClientTop       =   3105
   ClientWidth     =   6420
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
   Height          =   4065
   Left            =   2760
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3660
   ScaleWidth      =   6420
   Top             =   2760
   Width           =   6540
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   492
      Left            =   5160
      TabIndex        =   10
      Top             =   3120
      Width           =   1212
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      Height          =   492
      Left            =   3840
      TabIndex        =   14
      Top             =   3120
      Width           =   1212
   End
   Begin VB.TextBox Phone 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   12
      Top             =   2520
      Width           =   1692
   End
   Begin VB.TextBox ZIP 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   7
      Top             =   2160
      Width           =   1212
   End
   Begin VB.TextBox TheState 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   6
      Top             =   1800
      Width           =   612
   End
   Begin VB.TextBox City 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   5
      Top             =   1440
      Width           =   2772
   End
   Begin VB.TextBox Address 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   4
      Top             =   1080
      Width           =   4692
   End
   Begin VB.TextBox RealName 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   288
      Left            =   1560
      TabIndex        =   0
      Top             =   720
      Width           =   4692
   End
   Begin VB.Label UserName 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00FFFFFF&
      BackStyle       =   0  'Transparent
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   18
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   492
      Left            =   120
      TabIndex        =   13
      Top             =   120
      Width           =   6252
   End
   Begin VB.Label Label6 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Telephone:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   11
      Top             =   2520
      Width           =   1452
   End
   Begin VB.Label Label5 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "State/Province:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   9
      Top             =   1800
      Width           =   1452
   End
   Begin VB.Label Label4 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "ZIP Code:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   8
      Top             =   2160
      Width           =   1452
   End
   Begin VB.Label Label3 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "City/Town:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   3
      Top             =   1440
      Width           =   1452
   End
   Begin VB.Label Label2 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Address:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   2
      Top             =   1080
      Width           =   1452
   End
   Begin VB.Label Label1 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H00FFFFFF&
      BackStyle       =   0  'Transparent
      Caption         =   "Real name:"
      ForeColor       =   &H80000008&
      Height          =   252
      Left            =   0
      TabIndex        =   1
      Top             =   720
      Width           =   1452
   End
End
Attribute VB_Name = "Registration"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Unload Registration
    Load RoomPrompt
End Sub

Private Sub Form_Load()
    Show
    Registration.WindowState = 0
    Registration.Top = Int((MainWin.Height - Registration.Height) / 3)
    Registration.Left = Int((MainWin.Width - Registration.Width) / 2)
    DoEvents

    If begin_trans() = True Then
        serv_puts ("GREG _SELF_")
        a$ = serv_gets()
        If Left$(a$, 1) = "1" Then
            UserName.Caption = Right$(a$, Len(a$) - 4)
            b% = 0
            Do
                a$ = serv_gets()
                If b% = 2 Then RealName.Text = a$
                If b% = 3 Then Address.Text = a$
                If b% = 4 Then City.Text = a$
                If b% = 5 Then TheState.Text = a$
                If b% = 6 Then ZIP.Text = a$
                If b% = 7 Then Phone.Text = a$
                b% = b% + 1
                Loop Until a$ = "000"
            End If
        Call end_trans
        End If

End Sub

Private Sub save_button_Click()
    
    If begin_trans() = True Then
        serv_puts ("REGI")
        a$ = serv_gets()
        If Left$(a$, 1) = "4" Then
            serv_puts (RealName.Text)
            serv_puts (Address.Text)
            serv_puts (City.Text)
            serv_puts (TheState.Text)
            serv_puts (ZIP.Text)
            serv_puts (Phone.Text)
            serv_puts ("000")
            End If
        Call end_trans
        need_regis% = 0
        Unload Registration
        Load RoomPrompt
        End If

End Sub

