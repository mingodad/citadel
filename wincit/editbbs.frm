VERSION 4.00
Begin VB.Form EditBBS 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   1875
   ClientLeft      =   1005
   ClientTop       =   1830
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
   Height          =   2280
   Left            =   945
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   1875
   ScaleWidth      =   6420
   Top             =   1485
   Width           =   6540
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Cancel          =   -1  'True
      Caption         =   "Cancel"
      Height          =   495
      Left            =   5280
      TabIndex        =   6
      Top             =   1320
      Width           =   1095
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Save"
      Height          =   495
      Left            =   4080
      TabIndex        =   5
      Top             =   1320
      Width           =   1095
   End
   Begin Threed.SSFrame fPhoneOrAddress 
      Height          =   615
      Left            =   120
      TabIndex        =   1
      Top             =   600
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Internet address"
      Begin VB.TextBox uPhoneOrAddress 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   285
         Left            =   120
         TabIndex        =   4
         Text            =   "Text11"
         Top             =   240
         Width           =   6015
      End
   End
   Begin Threed.SSFrame Frame3D1 
      Height          =   615
      Left            =   120
      TabIndex        =   0
      Top             =   0
      Width           =   6255
      _Version        =   65536
      _ExtentX        =   11033
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Name"
      Begin VB.TextBox uName 
         Alignment       =   2  'Center
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   288
         Left            =   120
         TabIndex        =   3
         Text            =   "Text11"
         Top             =   240
         Width           =   6012
      End
   End
   Begin Threed.SSFrame fTCPport 
      Height          =   615
      Left            =   120
      TabIndex        =   2
      Top             =   1200
      Width           =   3255
      _Version        =   65536
      _ExtentX        =   5741
      _ExtentY        =   1085
      _StockProps     =   14
      Caption         =   "Port number"
      Begin VB.TextBox uTCPport 
         Appearance      =   0  'Flat
         BackColor       =   &H00C0C0C0&
         BorderStyle     =   0  'None
         Height          =   288
         Left            =   120
         TabIndex        =   7
         Top             =   240
         Width           =   3012
      End
   End
End
Attribute VB_Name = "EditBBS"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub cancel_button_Click()
    Load SelectBBS
    Unload EditBBS
End Sub

Private Sub Form_Load()
    uname.Text = StripTrailingWhiteSpace(CurrBBS.Name)
    If uname.Text = "newbbs000" Then uname.Text = "New BBS entry"
    uPhoneOrAddress = StripTrailingWhiteSpace(CurrBBS.PhoneOrAddress)

    EditBBS.Height = 2550
    uTCPport.Text = Str$(CurrBBS.TCPport)


End Sub

Private Sub save_button_Click()
    CurrBBS.Name = uname.Text
    CurrBBS.PhoneOrAddress = uPhoneOrAddress.Text
    On Error Resume Next
    CurrBBS.TCPport = Int(uTCPport)
    If Err <> 0 Then CurrBBS.TCPport = DefaultPort

    Load SelectBBS
    Unload EditBBS
End Sub


