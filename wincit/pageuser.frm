VERSION 4.00
Begin VB.Form PageUser 
   BorderStyle     =   1  'Fixed Single
   Caption         =   "Page another user"
   ClientHeight    =   5760
   ClientLeft      =   1545
   ClientTop       =   1725
   ClientWidth     =   6690
   ControlBox      =   0   'False
   Height          =   6165
   Left            =   1485
   LinkTopic       =   "Form1"
   MDIChild        =   -1  'True
   ScaleHeight     =   5760
   ScaleWidth      =   6690
   Top             =   1380
   Width           =   6810
   Begin VB.CommandButton send_button 
      Caption         =   "&Send message"
      Height          =   615
      Left            =   3120
      TabIndex        =   5
      Top             =   5040
      Width           =   1695
   End
   Begin VB.TextBox message_text 
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   3135
      Left            =   3120
      MaxLength       =   200
      MultiLine       =   -1  'True
      TabIndex        =   2
      Top             =   360
      Width           =   3495
   End
   Begin VB.ListBox online_users 
      Height          =   5325
      Left            =   120
      TabIndex        =   1
      Top             =   360
      Width           =   2895
   End
   Begin VB.CommandButton cancel_button 
      Caption         =   "&Cancel"
      Height          =   615
      Left            =   4920
      TabIndex        =   0
      Top             =   5040
      Width           =   1695
   End
   Begin VB.Label Label2 
      Caption         =   "Message to send:"
      Height          =   255
      Left            =   3120
      TabIndex        =   4
      Top             =   120
      Width           =   3375
   End
   Begin VB.Label Label1 
      Caption         =   "User to page:"
      Height          =   255
      Left            =   120
      TabIndex        =   3
      Top             =   120
      Width           =   1095
   End
End
Attribute VB_Name = "PageUser"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Private Sub cancel_button_Click()

Unload PageUser
Load RoomPrompt

End Sub


Private Sub Form_Load()

PageUser.Width = 6795
PageUser.Height = 6135
PageUser.WindowState = 0
PageUser.Top = Int((MainWin.Height - PageUser.Height) / 3)
PageUser.Left = Int((MainWin.Width - PageUser.Width) / 2)
Show

If begin_trans() = True Then
    serv_puts ("RWHO")
    a$ = serv_gets()
    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If a$ <> "000" Then
                online_users.AddItem (extract(a$, 1))
                End If
            Loop Until a$ = "000"
        End If
    end_trans
    End If

End Sub


Private Sub send_button_Click()

If Len(message_text.Text) < 1 Then GoTo dont_send

If online_users.ListIndex >= 0 Then
    mto$ = online_users.List(online_users.ListIndex)
Else
    GoTo dont_send
    End If
    
If begin_trans() <> True Then GoTo dont_send

buf$ = "SEXP " + mto$ + "|"

For a = 1 To Len(message_text.Text)
 If Asc(Mid$(message_text.Text, a, 1)) < 32 Then
    buf$ = buf$ + " "
 Else
    buf$ = buf$ + Mid$(message_text.Text, a, 1)
    End If
 Next a
 
 serv_puts (buf$)
 buf$ = serv_gets()
 If Left$(buf$, 1) <> "2" Then
    MsgBox Right$(buf$, Len(buf$) - 4), 16
    End If

end_trans

Unload PageUser
Load RoomPrompt

dont_send:

End Sub


