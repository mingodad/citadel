VERSION 4.00
Begin VB.Form Validation 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Validate new users"
   ClientHeight    =   4890
   ClientLeft      =   2415
   ClientTop       =   2880
   ClientWidth     =   6855
   ClipControls    =   0   'False
   ControlBox      =   0   'False
   BeginProperty Font 
      name            =   "MS Sans Serif"
      charset         =   0
      weight          =   700
      size            =   9.75
      underline       =   0   'False
      italic          =   0   'False
      strikethrough   =   0   'False
   EndProperty
   ForeColor       =   &H80000008&
   Height          =   5295
   Left            =   2355
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   4890
   ScaleWidth      =   6855
   Top             =   2535
   Width           =   6975
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   6
      Left            =   3480
      TabIndex        =   22
      Top             =   4320
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   5
      Left            =   1800
      TabIndex        =   21
      Top             =   4320
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   4
      Left            =   120
      TabIndex        =   20
      Top             =   4320
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   3
      Left            =   5160
      TabIndex        =   19
      Top             =   3720
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   2
      Left            =   3480
      TabIndex        =   18
      Top             =   3720
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   1
      Left            =   1800
      TabIndex        =   17
      Top             =   3720
      Width           =   1575
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
      Height          =   495
      Left            =   5160
      TabIndex        =   5
      Top             =   4320
      Width           =   1575
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Index           =   0
      Left            =   120
      TabIndex        =   8
      Top             =   3720
      Width           =   1575
   End
   Begin VB.Label Label8 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Please choose an access level for this user:"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   -1  'True
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   300
      Left            =   1080
      TabIndex        =   23
      Top             =   3240
      Width           =   4725
   End
   Begin VB.Label TheAxLevel 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Access level"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   16
      Top             =   2880
      Width           =   4695
   End
   Begin VB.Label Label7 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Current access:"
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
      TabIndex        =   15
      Top             =   2880
      Width           =   1455
   End
   Begin VB.Label Phone 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Phone"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   14
      Top             =   2520
      Width           =   4695
   End
   Begin VB.Label ZIP 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "ZIP"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   13
      Top             =   2160
      Width           =   4695
   End
   Begin VB.Label TheState 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "State"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   12
      Top             =   1800
      Width           =   4695
   End
   Begin VB.Label City 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "City"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   11
      Top             =   1440
      Width           =   4695
   End
   Begin VB.Label Address 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Address"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   10
      Top             =   1080
      Width           =   4695
   End
   Begin VB.Label RealName 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Real Name"
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   1560
      TabIndex        =   9
      Top             =   720
      Width           =   4695
   End
   Begin VB.Label UserName 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H00FFFFFF&
      BackStyle       =   0  'Transparent
      Caption         =   "UserName"
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
      TabIndex        =   7
      Top             =   120
      Width           =   6252
   End
   Begin VB.Label Label6 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Telephone:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   6
      Top             =   2520
      Width           =   1452
   End
   Begin VB.Label Label5 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "State/Province:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   4
      Top             =   1800
      Width           =   1452
   End
   Begin VB.Label Label4 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "ZIP Code:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   3
      Top             =   2160
      Width           =   1452
   End
   Begin VB.Label Label3 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "City/Town:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   2
      Top             =   1440
      Width           =   1452
   End
   Begin VB.Label Label2 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Address:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   1
      Top             =   1080
      Width           =   1452
   End
   Begin VB.Label Label1 
      Alignment       =   1  'Right Justify
      Appearance      =   0  'Flat
      BackColor       =   &H00FFFFFF&
      BackStyle       =   0  'Transparent
      Caption         =   "Real name:"
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
      Height          =   252
      Left            =   0
      TabIndex        =   0
      Top             =   720
      Width           =   1452
   End
End
Attribute VB_Name = "Validation"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Unload Validation
    Load RoomPrompt
End Sub

Private Sub Form_Load()
    Show
    Validation.WindowState = 0
    Validation.Top = Int((MainWin.Height - Validation.Height) / 3)
    Validation.Left = Int((MainWin.Width - Validation.Width) / 2)
    
    For i = 0 To 6
        save_button(i).Caption = Str$(i) + " (" + axdefs$(i) + ")"
        Next i
    
    Call GetNextUnregisteredUser

End Sub

Private Sub GetNextUnregisteredUser()

    RealName.Caption = ""
    Address.Caption = ""
    City.Caption = ""
    TheState.Caption = ""
    ZIP.Caption = ""
    Phone.Caption = ""
    TheAxLevel.Caption = ""
    
    If begin_trans() = True Then
    
        serv_puts ("GNUR")
        a$ = serv_gets()

        If Left$(a$, 1) = "2" Then
            Call end_trans
            'Unload Validation
            'Load RoomPrompt
            End If

        UserName.Caption = Right$(a$, Len(a$) - 4)
        
        If Left$(a$, 1) <> "3" Then
        
            For i = 0 To 6
                save_button(i).Enabled = False
                Next i
        

        Else

            serv_puts ("GREG " + UserName.Caption)
            a$ = serv_gets()
            If Left$(a$, 1) = "1" Then
                UserName.Caption = Right$(a$, Len(a$) - 4)
                b% = 0
                Do
                    a$ = serv_gets()
                    If b% = 2 Then RealName.Caption = a$
                    If b% = 3 Then Address.Caption = a$
                    If b% = 4 Then City.Caption = a$
                    If b% = 5 Then TheState.Caption = a$
                    If b% = 6 Then ZIP.Caption = a$
                    If b% = 7 Then Phone.Caption = a$
                    If b% = 8 Then TheAxLevel.Caption = a$ + " (" + axdefs$(Val(a$)) + ")"
                    b% = b% + 1
                    Loop Until a$ = "000"
                End If
            Call end_trans
            For i = 0 To 6
                save_button(i).Enabled = True
                Next i

            End If

        Call end_trans
        End If

End Sub

Private Sub save_button_Click(axl As Integer)
    
    If begin_trans() = True Then
        serv_puts ("VALI " + UserName.Caption + "|" + Str$(axl))
        a$ = serv_gets()
        Call end_trans
        If Left$(a$, 1) = "2" Then
            Call GetNextUnregisteredUser
        Else
            MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
            End If
        End If

End Sub

