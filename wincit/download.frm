VERSION 4.00
Begin VB.Form Download 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Download..."
   ClientHeight    =   3120
   ClientLeft      =   4140
   ClientTop       =   3630
   ClientWidth     =   5610
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
   Height          =   3525
   Left            =   4080
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3120
   ScaleWidth      =   5610
   Top             =   3285
   Width           =   5730
   Begin Threed.SSPanel info 
      Height          =   495
      Left            =   1200
      TabIndex        =   5
      Top             =   1320
      Width           =   4335
      _Version        =   65536
      _ExtentX        =   7646
      _ExtentY        =   873
      _StockProps     =   15
      Caption         =   "info"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin GaugeLib.Gauge progress 
      Height          =   255
      Left            =   240
      TabIndex        =   4
      Top             =   2040
      Width           =   5175
      _Version        =   65536
      _ExtentX        =   9128
      _ExtentY        =   450
      _StockProps     =   73
      Autosize        =   -1  'True
      NeedleWidth     =   1
   End
   Begin VB.CommandButton start_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Start download"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   495
      Left            =   2280
      TabIndex        =   3
      Top             =   2520
      Width           =   1815
   End
   Begin Threed.SSPanel DestFile 
      Height          =   495
      Left            =   1200
      TabIndex        =   2
      Top             =   720
      Width           =   4335
      _Version        =   65536
      _ExtentX        =   7646
      _ExtentY        =   873
      _StockProps     =   15
      Caption         =   "DestFile"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin Threed.SSPanel SourceFile 
      Height          =   495
      Left            =   1200
      TabIndex        =   1
      Top             =   120
      Width           =   4335
      _Version        =   65536
      _ExtentX        =   7646
      _ExtentY        =   873
      _StockProps     =   15
      Caption         =   "SourceFile"
      BeginProperty Font {0BE35203-8F91-11CE-9DE3-00AA004BB851} 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   492
      Left            =   4200
      TabIndex        =   0
      Top             =   2520
      Width           =   1332
   End
   Begin Threed.SSPanel Panel3D1 
      Height          =   495
      Left            =   120
      TabIndex        =   9
      Top             =   1920
      Width           =   5415
      _Version        =   65536
      _ExtentX        =   9551
      _ExtentY        =   873
      _StockProps     =   15
      BevelWidth      =   4
      BorderWidth     =   5
      BevelOuter      =   1
   End
   Begin MSComDlg.CommonDialog SaveAs 
      Left            =   1680
      Top             =   2520
      _Version        =   65536
      _ExtentX        =   847
      _ExtentY        =   847
      _StockProps     =   0
      DialogTitle     =   "Save As..."
      InitDir         =   "C:\"
   End
   Begin VB.Label Label3 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Info"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   300
      Left            =   120
      TabIndex        =   8
      Top             =   1440
      Width           =   420
   End
   Begin VB.Label Label2 
      Appearance      =   0  'Flat
      AutoSize        =   -1  'True
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Save As:"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   300
      Left            =   120
      TabIndex        =   7
      Top             =   840
      Width           =   945
   End
   Begin VB.Label Label1 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Source:"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   255
      Left            =   120
      TabIndex        =   6
      Top             =   240
      Width           =   975
   End
End
Attribute VB_Name = "Download"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim InProgress%
Dim Cancelled%

Private Sub cancel_button_Click()
    
If InProgress% = 1 Then
    Cancelled% = 1
Else
    Unload Download
    Load RoomPrompt
    End If

End Sub

Private Sub Form_Load()


    Show
    Download.WindowState = 0
    Download.Top = Int((MainWin.Height - Download.Height) / 3)
    Download.Left = Int((MainWin.Width - Download.Width) / 2)
    
    SourceFile.Caption = DownLoadFileName$

    SaveAs.filename = DownLoadFileName$
    SaveAs.Action = 2

    DestFile.Caption = SaveAs.filename
    


End Sub

Private Sub start_button_Click()

If begin_trans() = True Then
    Info.Caption = "Sending server command"
    
    serv_puts ("OPEN " + DownLoadFileName$)
    a$ = serv_gets()
    Info.Caption = a$

    If Left$(a$, 1) <> "2" Then
        FileLength& = (-1)
    Else
        FileLength& = Val(extract$(Right$(a$, Len(a$) - 4), 0))
        End If

    Info.Caption = "File len is " + Str$(FileLength&)
    Call end_trans
    End If

If FileLength& < 0 Then
    MsgBox Right$(a$, Len(a$) - 4), 16, "Error"
    Unload Download
    Load RoomPrompt
    End If

InProgress% = 1
Open DestFile.Caption For Output As #1
start_button.Enabled = False
start_button.Visible = False

Progress.min = 0
Progress.Max = FileLength& / 1024

GotBytes& = 0
Do While GotBytes& < FileLength&
    If Cancelled% = 1 Then GoTo ENDOFXFER
    NeedBytes& = 4096
    If (FileLength& - GotBytes&) < 4096 Then NeedBytes& = FileLength& - GotBytes&
    If begin_trans() = True Then
        serv_puts ("READ " + Str$(GotBytes&) + "|" + Str$(NeedBytes&))
        a$ = serv_gets()
        If Left$(a$, 1) = "6" Then
            b$ = serv_read(CInt(NeedBytes&))
            GotBytes& = GotBytes& + NeedBytes&
            Print #1, b$;
            End If
        Call end_trans
        End If
    Progress.Value = GotBytes& / 1024
    Info.Caption = "Received " + Str$(GotBytes&) + " of " + Str$(FileLength&) + " bytes"
    DoEvents
    Loop

ENDOFXFER:
Close #1
If begin_trans() = True Then
    serv_puts ("CLOS")
    a$ = serv_gets()
    cancel_button.Caption = "&OK!"
    InProgress% = 0
    Call end_trans
    End If

End Sub

