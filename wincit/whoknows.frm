VERSION 4.00
Begin VB.Form WhoKnowsRoom 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Who knows this room..."
   ClientHeight    =   3870
   ClientLeft      =   1530
   ClientTop       =   1440
   ClientWidth     =   6270
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
   Height          =   4275
   Left            =   1470
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3870
   ScaleWidth      =   6270
   Top             =   1095
   Width           =   6390
   Begin VB.CommandButton ok_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "OK"
      Height          =   492
      Left            =   4920
      TabIndex        =   1
      Top             =   3360
      Width           =   1332
   End
   Begin VB.ListBox TheList 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Height          =   3864
      Left            =   0
      Sorted          =   -1  'True
      TabIndex        =   0
      Top             =   0
      Width           =   4812
   End
End
Attribute VB_Name = "WhoKnowsRoom"
Attribute VB_Creatable = False
Attribute VB_Exposed = False

Private Sub Form_Load()


    Show
    WhoKnowsRoom.WindowState = 0
    WhoKnowsRoom.Top = Int((MainWin.Height - WhoKnowsRoom.Height) / 3)
    WhoKnowsRoom.Left = Int((MainWin.Width - WhoKnowsRoom.Width) / 2)
    DoEvents

If begin_trans() = True Then
    
    serv_puts ("WHOK")
    a$ = serv_gets()
    WhoKnowsRoom.Caption = Right$(a$, Len(a$) - 4)
    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If (a$ <> "000") Then TheList.AddItem (a$)
            Loop Until a$ = "000"
        End If
    Call end_trans
    End If

End Sub

Private Sub ok_button_Click()
    Unload WhoKnowsRoom
    Load RoomPrompt
End Sub

