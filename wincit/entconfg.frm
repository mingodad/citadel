VERSION 4.00
Begin VB.Form EnterConfiguration 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Account Configuration"
   ClientHeight    =   3660
   ClientLeft      =   2655
   ClientTop       =   3825
   ClientWidth     =   6420
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
   Height          =   4065
   Left            =   2595
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3660
   ScaleWidth      =   6420
   Top             =   3480
   Width           =   6540
   Begin VB.CheckBox Unlisted 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Be unlisted in the userlog"
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
      Height          =   495
      Left            =   120
      TabIndex        =   0
      Top             =   1680
      Width           =   6135
   End
   Begin VB.CheckBox LastOld 
      Appearance      =   0  'Flat
      BackColor       =   &H00C0C0C0&
      Caption         =   "Display last old message when reading new messages"
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
      Height          =   495
      Left            =   120
      TabIndex        =   1
      Top             =   960
      Width           =   6135
   End
   Begin VB.CommandButton cancel_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Cancel"
      Height          =   492
      Left            =   5160
      TabIndex        =   2
      Top             =   3120
      Width           =   1212
   End
   Begin VB.CommandButton save_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Save"
      Height          =   492
      Left            =   3840
      TabIndex        =   3
      Top             =   3120
      Width           =   1212
   End
End
Attribute VB_Name = "EnterConfiguration"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
' ScreenWidth and ScreenHeight don't get used in WinCit.
' We read them in when we do a GETU only so we can save
' them back when we save with SETU.   This way, if the
' user is also using the same server with the text client,
' his/her screen dimensions won't get frotzed.
Dim ScreenWidth%
Dim ScreenHeight%

' Here's what we're really interested in.
Dim UserFlags%

Private Sub cancel_button_Click()
    Unload EnterConfiguration
    Load RoomPrompt
End Sub

Private Sub Form_Load()
    Show
    EnterConfiguration.WindowState = 0
    EnterConfiguration.Top = Int((MainWin.Height - EnterConfiguration.Height) / 3)
    EnterConfiguration.Left = Int((MainWin.Width - EnterConfiguration.Width) / 2)
    DoEvents

    a$ = "        "

    If begin_trans() = True Then
        serv_puts ("GETU")
        a$ = serv_gets()
        Call end_trans
        End If

    If Left$(a$, 1) = "2" Then
        b$ = Right$(a$, Len(a$) - 4)
        ScreenWidth% = Val(extract$(b$, 0))
        ScreenHeight% = Val(extract$(b$, 1))
        UserFlags% = Val(extract$(b$, 2))
    Else
        save_button_enabled = False
        MsgBox Right$(a$, Len(a$) - 4), 16
        End If

    If (UserFlags% And 16) Then
        LastOld.Value = 1
    Else
        LastOld.Value = 0
        End If

    If (UserFlags% And 64) Then
        Unlisted.Value = 1
    Else
        Unlisted.Value = 0
        End If

End Sub

Private Sub save_button_Click()
    
    UserFlags% = UserFlags% Or 16 Or 64

    If LastOld.Value = 0 Then UserFlags% = UserFlags% - 16
    If Unlisted.Value = 0 Then UserFlags% = UserFlags% - 64
    
    a$ = "SETU " + Str$(ScreenWidth%) + "|" + Str$(ScreenHeight%) + "|" + Str$(UserFlags%)

    If begin_trans() = True Then
        
        serv_puts (a$)
        a$ = serv_gets()
        If Left$(a$, 1) <> "2" Then
            MsgBox Right$(a$, Len(a$) - 4), 16
            End If
        Call end_trans
        Unload EnterConfiguration
        Load RoomPrompt
        End If

End Sub

