VERSION 4.00
Begin VB.Form UserList 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "User Listing"
   ClientHeight    =   5430
   ClientLeft      =   1440
   ClientTop       =   1425
   ClientWidth     =   6270
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
   Height          =   5835
   Left            =   1380
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   5430
   ScaleWidth      =   6270
   Top             =   1080
   Width           =   6390
   WindowState     =   2  'Maximized
   Begin VB.CommandButton ok_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "OK"
      Height          =   492
      Left            =   4920
      TabIndex        =   1
      Top             =   4920
      Width           =   1332
   End
   Begin VB.ListBox TheList 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   400
         size            =   12
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   5424
      Left            =   0
      Sorted          =   -1  'True
      TabIndex        =   0
      Top             =   0
      Width           =   4812
   End
End
Attribute VB_Name = "UserList"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub Form_Load()


    Show
    UserList.WindowState = 0
    UserList.Top = Int((MainWin.Height - UserList.Height) / 3)
    UserList.Left = Int((MainWin.Width - UserList.Width) / 2)
    DoEvents

If begin_trans() = True Then
    
    serv_puts ("LIST")
    a$ = serv_gets()
    If Left$(a$, 1) = "1" Then
        UserList.Caption = "Total number of users: " + Right$(a$, Len(a$) - 4)
    Else
        UserList.Caption = Right$(a$, Len(a$) - 4)
        End If
    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If (a$ <> "000") Then TheList.AddItem (extract(a$, 0))
            DoEvents
            Loop Until a$ = "000"
        End If
    Call end_trans
    End If

End Sub

Private Sub ok_button_Click()
    Unload UserList
    Load RoomPrompt
End Sub

