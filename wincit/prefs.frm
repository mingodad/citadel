VERSION 4.00
Begin VB.Form Preferences 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   4290
   ClientLeft      =   1530
   ClientTop       =   1800
   ClientWidth     =   5865
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
   Height          =   4695
   Left            =   1470
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   4290
   ScaleWidth      =   5865
   Top             =   1455
   Width           =   5985
   Begin Threed.SSFrame Frame3D1 
      Height          =   855
      Left            =   120
      TabIndex        =   13
      Top             =   2880
      Width           =   5655
      _Version        =   65536
      _ExtentX        =   9975
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Misc Preferences"
      ForeColor       =   0
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Begin VB.CheckBox MailNotice 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Display message at login if you have new private mail"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   14
         Top             =   360
         Width           =   5412
      End
   End
   Begin VB.CommandButton Save 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Save"
      Height          =   372
      Left            =   3480
      TabIndex        =   10
      Top             =   3840
      Width           =   1092
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Cancel          =   -1  'True
      Caption         =   "Cancel"
      Height          =   372
      Left            =   4680
      TabIndex        =   9
      Top             =   3840
      Width           =   1092
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   1455
      Left            =   120
      TabIndex        =   0
      Top             =   0
      Width           =   5655
      _Version        =   65536
      _ExtentX        =   9975
      _ExtentY        =   2566
      _StockProps     =   14
      Caption         =   "Default action when a room is double-clicked"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Begin VB.OptionButton DefaultIsAbandon 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "<A>bandon (set last-read pointer where you stopped reading)"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   3
         Top             =   1080
         Width           =   5412
      End
      Begin VB.OptionButton DefaultIsSkip 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "<S>kip (leave all messages marked unread)"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   255
         Left            =   120
         TabIndex        =   2
         Top             =   720
         Width           =   5055
      End
      Begin VB.OptionButton DefaultIsGoto 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "<G>oto (mark all messages as read)"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   1
         Top             =   360
         Width           =   4452
      End
   End
   Begin Threed.SSFrame Frame3D3 
      Height          =   1335
      Left            =   120
      TabIndex        =   4
      Top             =   1440
      Width           =   5655
      _Version        =   65536
      _ExtentX        =   9975
      _ExtentY        =   2355
      _StockProps     =   14
      Caption         =   "Font preferences"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Begin VB.CommandButton ChangeFixedFont 
         Appearance      =   0  'Flat
         BackColor       =   &H80000005&
         Caption         =   "Change"
         Height          =   375
         Left            =   4680
         TabIndex        =   8
         Top             =   840
         Width           =   855
      End
      Begin VB.CommandButton ChangeVariFont 
         Appearance      =   0  'Flat
         BackColor       =   &H80000005&
         Caption         =   "Change"
         Height          =   375
         Left            =   4680
         TabIndex        =   7
         Top             =   360
         Width           =   855
      End
      Begin VB.Label FixedFontName 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H80000005&
         BackStyle       =   0  'Transparent
         Caption         =   "Fixed Font Name"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   375
         Left            =   1560
         TabIndex        =   11
         Top             =   915
         Width           =   3135
      End
      Begin VB.Label VariFontName 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H80000005&
         BackStyle       =   0  'Transparent
         Caption         =   "Variable Font Name"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   700
            size            =   12
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   375
         Left            =   1560
         TabIndex        =   12
         Top             =   435
         Width           =   3135
      End
      Begin VB.Label Label2 
         Alignment       =   1  'Right Justify
         Appearance      =   0  'Flat
         AutoSize        =   -1  'True
         BackColor       =   &H00C0C0C0&
         Caption         =   "Fixed width font:"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   195
         Left            =   120
         TabIndex        =   6
         Top             =   960
         Width           =   1155
      End
      Begin VB.Label Label1 
         Alignment       =   1  'Right Justify
         Appearance      =   0  'Flat
         AutoSize        =   -1  'True
         BackColor       =   &H00C0C0C0&
         Caption         =   "Variable width font:"
         BeginProperty Font 
            name            =   "MS Sans Serif"
            charset         =   0
            weight          =   400
            size            =   8.25
            underline       =   0   'False
            italic          =   0   'False
            strikethrough   =   0   'False
         EndProperty
         ForeColor       =   &H80000008&
         Height          =   195
         Left            =   120
         TabIndex        =   5
         Top             =   480
         Width           =   1350
      End
   End
   Begin MSComDlg.CommonDialog CMDialog1 
      Left            =   120
      Top             =   3840
      _Version        =   65536
      _ExtentX        =   847
      _ExtentY        =   847
      _StockProps     =   0
      DialogTitle     =   "Change Font"
      Max             =   32767
   End
End
Attribute VB_Name = "Preferences"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim varifontsize

Private Sub cancel_button_Click()

    Load SelectBBS
    Unload Preferences

End Sub

Private Sub ChangeFixedFont_Click()
    
    On Error Resume Next
    CMDialog1.min = 0
    CMDialog1.Max = 32767
    CMDialog1.DialogTitle = "Font for Fixed-format messages"
    CMDialog1.Flags = &H4001&
    CMDialog1.FontName = fixedfontname
    CMDialog1.Action = 4
    If Err <> 0 Then
        MsgBox "Can't open font dialog", 48
        Return
        End If
    fixedfontname.FontName = CMDialog1.FontName
    fixedfontname.Caption = fixedfontname.FontName

End Sub

Private Sub ChangeVariFont_Click()
    
    On Error Resume Next
    CMDialog1.min = 0
    CMDialog1.Max = 32767
    CMDialog1.DialogTitle = "Font for vari-format messages"
    CMDialog1.Flags = &H1&
    CMDialog1.FontName = varifontname.FontName
    CMDialog1.FontSize = varifontsize
    CMDialog1.Action = 4
    If Err <> 0 Then
        MsgBox "Can't open font dialog", 48
        Return
        End If
    varifontname.FontName = CMDialog1.FontName
    varifontsize = CMDialog1.FontSize
    varifontname.Caption = varifontname.FontName + " " + Str$(varifontsize)
End Sub

Private Sub Form_Load()

    On Error Resume Next
    
    varifontname.FontName = GetPrivateProfileVBString("Preferences", "VariFontName", "MS Sans Serif", "WINCIT.INI")
    varifontsize = GetPrivateProfileInt("Preferences", "VariFontSize", 10, "WINCIT.INI")
    varifontname.FontBold = False
    varifontname.FontItalic = False
    varifontname.Caption = varifontname.FontName + " " + Str$(varifontsize)

    fixedfontname.FontName = GetPrivateProfileVBString("Preferences", "FixedFontName", "Courier New", "WINCIT.INI")
    fixedfontname.FontBold = False
    fixedfontname.FontItalic = False
    fixedfontname.Caption = fixedfontname.FontName

    a$ = GetPrivateProfileVBString("Preferences", "DoubleClickAction", "GOTO", "WINCIT.INI")
    DefaultIsGoto.Value = True
    If a$ = "SKIP" Then DefaultIsSkip.Value = True
    If a$ = "ABANDON" Then DefaultIsAbandon.Value = True

    b% = GetPrivateProfileInt("Preferences", "MailNotice", 1, "WINCIT.INI")
    If (b% > 0) Then
        MailNotice.Value = 1
    Else
        MailNotice.Value = 0
        End If

End Sub

Private Sub Save_Click()

    a% = WritePrivateProfileString("Preferences", "VariFontName", varifontname.FontName, "WINCIT.INI")
    a% = WritePrivateProfileString("Preferences", "VariFontSize", Str$(varifontsize), "WINCIT.INI")
    
    a% = WritePrivateProfileString("Preferences", "FixedFontName", fixedfontname.FontName, "WINCIT.INI")
    
    If DefaultIsGoto.Value = True Then a% = WritePrivateProfileString("Preferences", "DoubleClickAction", "GOTO", "WINCIT.INI")
    If DefaultIsSkip.Value = True Then a% = WritePrivateProfileString("Preferences", "DoubleClickAction", "SKIP", "WINCIT.INI")
    If DefaultIsAbandon.Value = True Then a% = WritePrivateProfileString("Preferences", "DoubleClickAction", "ABANDON", "WINCIT.INI")

    If MailNotice.Value = 1 Then
        a% = WritePrivateProfileString("Preferences", "MailNotice", "1", "WINCIT.INI")
    Else
        a% = WritePrivateProfileString("Preferences", "MailNotice", "0", "WINCIT.INI")
        End If

    Load SelectBBS
    Unload Preferences

End Sub

