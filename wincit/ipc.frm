VERSION 4.00
Begin VB.Form IPC 
   Appearance      =   0  'Flat
   BackColor       =   &H80000005&
   BorderStyle     =   1  'Fixed Single
   ClientHeight    =   3075
   ClientLeft      =   2130
   ClientTop       =   1575
   ClientWidth     =   5895
   ControlBox      =   0   'False
   BeginProperty Font 
      name            =   "System"
      charset         =   0
      weight          =   700
      size            =   9.75
      underline       =   0   'False
      italic          =   0   'False
      strikethrough   =   0   'False
   EndProperty
   ForeColor       =   &H80000008&
   Height          =   3480
   Left            =   2070
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3075
   ScaleWidth      =   5895
   Top             =   1230
   Width           =   6015
   Begin VB.Timer md_timeout 
      Left            =   120
      Top             =   2520
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Cancel"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   492
      Left            =   4680
      TabIndex        =   1
      Top             =   2520
      Width           =   1092
   End
   Begin VB.TextBox DebugWin 
      Appearance      =   0  'Flat
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   8.25
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   2052
      Left            =   0
      MultiLine       =   -1  'True
      ScrollBars      =   2  'Vertical
      TabIndex        =   0
      Top             =   360
      Width           =   5892
   End
   Begin IPOCXLib.IPocx TCP 
      Left            =   600
      Top             =   2520
      _Version        =   65536
      _ExtentX        =   2143
      _ExtentY        =   873
      _StockProps     =   0
      HostAddr        =   ""
      HostPort        =   0
      LocalAddr       =   ""
      LocalPort       =   0
   End
   Begin VB.Label StatMsg 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H80000005&
      Caption         =   "Connecting to server, please wait..."
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
      Height          =   300
      Left            =   0
      TabIndex        =   2
      Top             =   0
      Width           =   5892
      WordWrap        =   -1  'True
   End
End
Attribute VB_Name = "IPC"
Attribute VB_Creatable = False
Attribute VB_Exposed = False


Private Sub cancel_button_Click()
    Cancelled = True
    DebugWin.Text = "CANCELLED!"
End Sub

Private Sub Form_Load()
    
    Cancelled = False
    IPC.Width = 5988
    IPC.Height = 3492
    IPC.Top = Int((MainWin.Height - IPC.Height) / 3)
    IPC.Left = Int((MainWin.Width - IPC.Width) / 2)
    Show
    DoEvents

  
TCPConnect:
    tcpconnected = False
    h$ = StripTrailingWhiteSpace(CurrBBS.PhoneOrAddress)
    p$ = StripTrailingWhiteSpace(Str$(CurrBBS.TCPport))

    On Error Resume Next
    statmsg.Caption = "Trying <" + h$ + "> port <" + p$ + ">"
    TCP.HostAddr = h$
    TCP.HostPort = p$
    TCP.Connect
    TCPinbuf$ = ""

    If Cancelled = True Then GoTo endload

StartServer:
    statmsg.Caption = serv_gets()

    IPC.Width = 0
    IPC.Height = 0

    DoubleClickAction$ = GetPrivateProfileVBString("Preferences", "DoubleClickAction", "GOTO", "WINCIT.INI")
    Load CitUser
    md_timeout.Interval = KeepAlive
    md_timeout.Enabled = True
    InTrans = False
    SaveNewCount = 0
    SaveOldCount = 0

endload:
    If Cancelled = True Then
        TCP.Connected = False
        IPC.Width = 0
        IPC.Height = 0
        Load SelectBBS

        End If
End Sub

Private Sub Form_Unload(Cancel As Integer)

    If tcpconnected = True Then
        tcpconnected = False
        End If

End Sub

Private Sub md_timeout_Timer()
    md_timeout.Enabled = False
    If begin_trans() = True Then
        serv_puts ("NOOP")
        a$ = serv_gets()
        If Left$(a$, 1) <> "2" Then
            MsgBox a$, 48
            End If
        If Mid$(a$, 4, 1) = "*" Then
            serv_puts ("PEXP")
            a$ = serv_gets()
            If Left$(a$, 1) = "1" Then
                ex$ = ""
                Do
                    a$ = serv_gets()
                    If a$ <> "000" Then ex$ = ex$ + a$ + Chr$(13) + Chr$(10)
                    Loop Until a$ = "000"
                MsgBox ex$, 0, "Express message"
                End If
            End If
        Call end_trans
        End If
    md_timeout.Interval = KeepAlive
    md_timeout.Enabled = True
End Sub


Private Sub TCP_DataReceived(ByVal data As String, ByVal l As Long)

TCPinbuf$ = TCPinbuf$ + data

End Sub

Private Sub TCP_Disconnected()

tcpconnected = False

End Sub


