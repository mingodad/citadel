VERSION 4.00
Begin VB.Form ModemSetup 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   4170
   ClientLeft      =   2295
   ClientTop       =   1710
   ClientWidth     =   5370
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
   Height          =   4575
   Left            =   2235
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   4170
   ScaleWidth      =   5370
   Top             =   1365
   Width           =   5490
   Begin VB.CommandButton Cancel_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Cancel"
      Height          =   492
      Left            =   3960
      TabIndex        =   23
      Top             =   3600
      Width           =   1332
   End
   Begin VB.CommandButton Save_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Save"
      Height          =   492
      Left            =   2520
      TabIndex        =   22
      Top             =   3600
      Width           =   1332
   End
   Begin Threed.SSFrame Frame3D7 
      Height          =   615
      Left            =   120
      TabIndex        =   21
      Top             =   2880
      Width           =   5175
      _Version        =   65536
      _ExtentX        =   9128
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Dial Command Prefix"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.TextBox uDialPrefix 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   288
         Left            =   120
         TabIndex        =   25
         Top             =   240
         Width           =   4932
      End
   End
   Begin Threed.SSFrame Frame3D6 
      Height          =   615
      Left            =   120
      TabIndex        =   20
      Top             =   2280
      Width           =   5175
      _Version        =   65536
      _ExtentX        =   9128
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Modem Initialization String"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.TextBox uInitString 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   288
         Left            =   120
         TabIndex        =   24
         Top             =   240
         Width           =   4932
      End
   End
   Begin Threed.SSFrame Frame3D5 
      Height          =   1095
      Left            =   4320
      TabIndex        =   16
      Top             =   1200
      Width           =   975
      _Version        =   65536
      _ExtentX        =   1720
      _ExtentY        =   1931
      _StockProps     =   14
      Caption         =   "Stop Bits"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.OptionButton SB2 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "2"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   18
         Top             =   720
         Width           =   732
      End
      Begin VB.OptionButton SB1 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "1"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   17
         Top             =   360
         Width           =   732
      End
   End
   Begin Threed.SSFrame Frame3D4 
      Height          =   1095
      Left            =   4320
      TabIndex        =   6
      Top             =   0
      Width           =   975
      _Version        =   65536
      _ExtentX        =   1720
      _ExtentY        =   1931
      _StockProps     =   14
      Caption         =   "Data Bits"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.OptionButton DB7 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "7"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   15
         Top             =   720
         Width           =   612
      End
      Begin VB.OptionButton DB8 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "8"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   14
         Top             =   360
         Width           =   612
      End
   End
   Begin Threed.SSFrame Frame3D3 
      Height          =   2295
      Left            =   3120
      TabIndex        =   4
      Top             =   0
      Width           =   1095
      _Version        =   65536
      _ExtentX        =   1931
      _ExtentY        =   4048
      _StockProps     =   14
      Caption         =   "Parity"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.OptionButton ParSpace 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Space"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   13
         Top             =   1800
         Width           =   852
      End
      Begin VB.OptionButton ParMark 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Mark"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   12
         Top             =   1440
         Width           =   852
      End
      Begin VB.OptionButton ParOdd 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Odd"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   11
         Top             =   1080
         Width           =   852
      End
      Begin VB.OptionButton ParEven 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Even"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   10
         Top             =   720
         Width           =   852
      End
      Begin VB.OptionButton ParNone 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "None"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   5
         Top             =   360
         Width           =   852
      End
   End
   Begin Threed.SSFrame Frame3D2 
      Height          =   855
      Left            =   1320
      TabIndex        =   2
      Top             =   0
      Width           =   1695
      _Version        =   65536
      _ExtentX        =   2990
      _ExtentY        =   1508
      _StockProps     =   14
      Caption         =   "Speed"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.ComboBox SpeedSel 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Height          =   315
         Left            =   120
         TabIndex        =   3
         Top             =   360
         Width           =   1452
      End
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   2295
      Left            =   120
      TabIndex        =   0
      Top             =   0
      Width           =   1095
      _Version        =   65536
      _ExtentX        =   1931
      _ExtentY        =   4048
      _StockProps     =   14
      Caption         =   "COM Port"
      ForeColor       =   0
      Font3D          =   1
      Begin VB.OptionButton Com5 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Com 5"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   19
         Top             =   1800
         Width           =   852
      End
      Begin VB.OptionButton Com4 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Com 4"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   9
         Top             =   1440
         Width           =   852
      End
      Begin VB.OptionButton Com3 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Com 3"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   8
         Top             =   1080
         Width           =   852
      End
      Begin VB.OptionButton Com2 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Com 2"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   7
         Top             =   720
         Width           =   852
      End
      Begin VB.OptionButton Com1 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         Caption         =   "Com 1"
         ForeColor       =   &H80000008&
         Height          =   252
         Left            =   120
         TabIndex        =   1
         Top             =   360
         Width           =   852
      End
   End
End
Attribute VB_Name = "ModemSetup"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Unload ModemSetup
    Load SelectBBS
End Sub

Private Sub Form_Load()

    Select Case comport%
        Case 1
            Com1.Value = True
        Case 2
            Com2.Value = True
        Case 3
            Com3.Value = True
        Case 4
            Com4.Value = True
        Case 5
            Com5.Value = True
        End Select

    SpeedSel.Text = Str$(comspeed&)
    SpeedSel.AddItem ("300")
    SpeedSel.AddItem ("1200")
    SpeedSel.AddItem ("2400")
    SpeedSel.AddItem ("4800")
    SpeedSel.AddItem ("9600")
    SpeedSel.AddItem ("19200")

    Select Case ComParity$
        Case "N"
            ParNone.Value = True
        Case "E"
            ParEven.Value = True
        Case "O"
            ParOdd.Value = True
        Case "M"
            ParMark.Value = True
        Case "S"
            ParSpace.Value = True
            End Select

    Select Case comdatabits%
        Case 8
            DB8.Value = True
        Case 7
            DB7.Value = True
            End Select

    Select Case comstopbits%
        Case 1
            SB1.Value = True
        Case 2
            SB2.Value = True
            End Select

    uInitString.Text = cominitstring$
    uDialPrefix.Text = comdialprefix$

End Sub

Private Sub Form_Resize()

    ModemSetup.Left = Abs(Int((MainWin.Width - ModemSetup.Width) / 2))
    ModemSetup.Top = Abs(Int((MainWin.Height - ModemSetup.Height) / 3))


End Sub

Private Sub save_button_Click()
    
    If Com1.Value = True Then comport% = 1
    If Com2.Value = True Then comport% = 2
    If Com3.Value = True Then comport% = 3
    If Com4.Value = True Then comport% = 4
    If Com5.Value = True Then comport% = 5

    comspeed& = CLng(SpeedSel.Text)

    If ParNone.Value = True Then ComParity$ = "N"
    If ParEven.Value = True Then ComParity$ = "E"
    If ParOdd.Value = True Then ComParity$ = "O"
    If ParMark.Value = True Then ComParity$ = "M"
    If ParSpace.Value = True Then ComParity$ = "S"

    If DB8.Value = True Then comdatabits% = 8
    If DB7.Value = True Then comdatabits% = 7

    If SB1.Value = True Then comstopbits% = 1
    If SB2.Value = True Then comstopbits% = 2
    
    cominitstring$ = uInitString.Text
    comdialprefix$ = uDialPrefix.Text

    a% = WritePrivateProfileString("Modem Setup", "ComPort", Str$(comport%), "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "Speed", CStr(comspeed&), "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "Parity", ComParity$, "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "DataBits", Str$(comdatabits%), "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "StopBits", Str$(comstopbits%), "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "InitString", cominitstring$, "WINCIT.INI")
    a% = WritePrivateProfileString("Modem Setup", "DialPrefix", comdialprefix$, "WINCIT.INI")

    Unload ModemSetup
    Load SelectBBS
End Sub

