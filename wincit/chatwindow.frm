VERSION 4.00
Begin VB.Form ChatWindow 
   AutoRedraw      =   -1  'True
   BorderStyle     =   0  'None
   Caption         =   "Chat"
   ClientHeight    =   5940
   ClientLeft      =   1485
   ClientTop       =   1635
   ClientWidth     =   6690
   ControlBox      =   0   'False
   Height          =   6345
   Left            =   1425
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   5940
   ScaleWidth      =   6690
   Top             =   1290
   Width           =   6810
   Begin VB.CommandButton list_button 
      Caption         =   "&List chat users"
      Height          =   495
      Left            =   3240
      TabIndex        =   3
      Top             =   5280
      Width           =   1335
   End
   Begin VB.CommandButton quit_button 
      Caption         =   "Exit Chat Mode"
      Height          =   495
      Left            =   4680
      TabIndex        =   2
      Top             =   5280
      Width           =   1335
   End
   Begin VB.Timer ChatRefresh 
      Interval        =   2000
      Left            =   120
      Top             =   5280
   End
   Begin VB.TextBox Outgoing 
      Height          =   375
      Left            =   120
      MultiLine       =   -1  'True
      TabIndex        =   1
      Top             =   4680
      Width           =   6495
   End
   Begin VB.TextBox Incoming 
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   9.75
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   4095
      Left            =   120
      Locked          =   -1  'True
      MultiLine       =   -1  'True
      TabIndex        =   0
      Top             =   120
      Width           =   6495
   End
End
Attribute VB_Name = "ChatWindow"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Dim LastUser$

Private Sub ChatRefresh_Timer()

Top:
If InStr(TCPinbuf$, Chr$(10)) > 0 Then
    a$ = serv_gets()
    If a$ = "000" Then
        ChatRefresh.Enabled = False
        end_trans
        DoEvents
        Unload ChatWindow
        Load RoomPrompt
        GoTo TheEnd
        End If

    p = InStr(a$, "|")
    If p < 1 Then
        a$ = "?|" + a$
        p = 2
        End If
    ThisUser$ = Left$(a$, p - 1)
    a$ = Right$(a$, Len(a$) - p)
    If a$ = "NOOP" Then GoTo Top
    If (ThisUser$ <> LastUser$) Then
        Incoming.Text = Incoming.Text + Chr$(13) + Chr$(10) + ThisUser$ + ":"
        LastUser$ = ThisUser$
        End If
    Incoming.Text = Incoming.Text + a$ + Chr$(13) + Chr$(10)
    
    ' Count the lines and strip down to 28
    nl = 0
    For b = 1 To Len(Incoming.Text)
        If Mid$(Incoming.Text, b, 1) = Chr$(13) Then nl = nl + 1
        Next b
    Do While nl > 28
        np = InStr(Incoming.Text, Chr$(13))
        Incoming.Text = Right$(Incoming.Text, Len(Incoming.Text) - np - 2)
        nl = nl - 1
        Loop
        
    GoTo Top
    End If
    

ChatRefresh.Interval = 2000
ChatRefresh.Enabled = True

TheEnd: Rem - go here when done

End Sub


Private Sub Form_Load()

MainWin.MousePointer = 0
Show
LastUser$ = ""
Outgoing.SetFocus

End Sub





Private Sub Form_Resize()

    ChatWindow.Left = 0
    ChatWindow.Top = 0
    ChatWindow.Width = MainWin.Width - 200
    ChatWindow.Height = MainWin.Height - 450
    
    Incoming.Left = 100
    Incoming.Width = ChatWindow.Width - 200
    Incoming.Top = 100
    Incoming.Height = ChatWindow.Height - 1200
    
    Outgoing.Left = 100
    Outgoing.Width = ChatWindow.Width - 200
    Outgoing.Top = ChatWindow.Height - 1100
    Outgoing.Height = 400

    quit_button.Top = ChatWindow.Height - 100 - quit_button.Height
    quit_button.Left = ChatWindow.Width - 100 - quit_button.Width
    list_button.Top = quit_button.Top
    list_button.Left = quit_button.Left - quit_button.Width - 100

End Sub


Private Sub list_button_Click()

serv_puts ("/who")

End Sub

Private Sub Outgoing_Change()

a = InStr(Outgoing.Text, Chr$(13) + Chr$(10))
If (a > 0) Then
    serv_puts (Left$(Outgoing.Text, a - 1))
    Outgoing.Text = Right$(Outgoing.Text, Len(Outgoing.Text) - a - 1)
    End If

If (Len(Outgoing.Text) > 55) Or (Len(Outgoing.Text) > 48 And Right$(Outgoing.Text, 1) = " ") Then
    serv_puts (Outgoing.Text)
    Outgoing.Text = ""
    End If

End Sub


Private Sub quit_button_Click()

    serv_puts ("/QUIT")
    

End Sub


