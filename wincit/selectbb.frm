VERSION 4.00
Begin VB.Form SelectBBS 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   ClientHeight    =   3120
   ClientLeft      =   2130
   ClientTop       =   3780
   ClientWidth     =   7500
   ControlBox      =   0   'False
   FillColor       =   &H00C0C0C0&
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
   Left            =   2070
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   3120
   ScaleWidth      =   7500
   Top             =   3435
   Width           =   7620
   Begin VB.CommandButton Prefs_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Preferences..."
      Height          =   492
      Left            =   6000
      TabIndex        =   5
      Top             =   2520
      Width           =   1452
   End
   Begin VB.CommandButton Call_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Connect"
      Height          =   495
      Left            =   4560
      TabIndex        =   4
      Top             =   2520
      Width           =   1215
   End
   Begin VB.CommandButton Delete_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Delete"
      Height          =   495
      Left            =   3120
      TabIndex        =   3
      Top             =   2520
      Width           =   1215
   End
   Begin VB.CommandButton Edit_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "Edit"
      Height          =   495
      Left            =   1560
      TabIndex        =   2
      Top             =   2520
      Width           =   1335
   End
   Begin VB.CommandButton New_Button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "New"
      Height          =   495
      Left            =   120
      TabIndex        =   1
      Top             =   2520
      Width           =   1215
   End
   Begin VB.ListBox List1 
      Appearance      =   0  'Flat
      BeginProperty Font 
         name            =   "Arial"
         charset         =   0
         weight          =   700
         size            =   9
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      Height          =   1830
      Left            =   120
      TabIndex        =   0
      Top             =   120
      Width           =   7335
   End
End
Attribute VB_Name = "SelectBBS"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim myDirectory(30) As BBSdir
Dim maxEntries%

Private Sub call_button_click()
    
    If List1.ListIndex >= 0 Then

        CurrBBS = myDirectory(List1.ListIndex)

        Cancelled = False
        Unload SelectBBS
        Unload IPC
        Load IPC
        End If

End Sub

Private Sub Delete_Button_Click()
    
    If List1.ListIndex >= 0 Then
        If (MsgBox("Are you sure you wish to delete this system from your list?", 4 + 48 + 256) = 6) Then
            For a% = List1.ListIndex To maxEntries% - 1
                myDirectory(a%) = myDirectory(a% + 1)
                Next

            maxEntries% = maxEntries% - 1

            End If
        
        Call SaveDialingDirectory
        Call display_directory
        End If

End Sub

Private Sub display_directory()
    List1.Clear
    If maxEntries > 0 Then
        For a% = 0 To maxEntries - 1
            List1.AddItem (myDirectory(a%).Name)
            Next a%
        End If

End Sub

Private Sub edit_button_click()
    If List1.ListIndex >= 0 Then
        CurrBBS = myDirectory(List1.ListIndex)
        editedBBSnum = List1.ListIndex
        Load EditBBS
        Unload SelectBBS
        End If

End Sub

Private Sub Form_Load()

    Cancelled = True
    MainWin.Caption = "Citadel/UX Client for Windows"

    Show
    SelectBBS.Width = Int(MainWin.Width * 0.9)
    SelectBBS.Height = Int(MainWin.Height * 0.7)
    SelectBBS.Left = Int((MainWin.Width - SelectBBS.Width) / 2)
    SelectBBS.Top = Int((MainWin.Height - SelectBBS.Height) / 4)
    
   
    ' Initialize the dialing directory
    maxEntries% = 0

    Call LoadDialingDirectory

End Sub

Private Sub Form_Resize()

    new_button.Top = Abs(SelectBBS.Height - 996)
    edit_button.Top = Abs(SelectBBS.Height - 996)
    delete_button.Top = Abs(SelectBBS.Height - 996)
    call_button.Top = Abs(SelectBBS.Height - 996)
    prefs_button.Top = Abs(SelectBBS.Height - 996)
    List1.Height = Abs(SelectBBS.Height - 1212 - List1.Top + 120)
    List1.Width = Abs(SelectBBS.Width - 288)

End Sub

Private Sub List1_DblClick()
    Call call_button_click
End Sub

Private Sub List1_KeyPress(keyascii As Integer)
    If keyascii = 13 Then Call call_button_click
End Sub

Private Sub LoadDialingDirectory()
    On Error Resume Next
    Open "dialing.dir" For Input As #1
    If Err = 0 Then
        maxEntries% = 0
        Do
            a% = maxEntries
            Input #1, myDirectory(a%).Name, myDirectory(a%).PhoneOrAddress, myDirectory(a%).TCPport
            If Err = 0 Then maxEntries = maxEntries + 1
            Loop Until Err <> 0
        Close #1
        End If


    b% = 0
    If editedBBSnum >= 0 Then
        myDirectory(editedBBSnum) = CurrBBS
        editedBBSnum = (-1)
        b% = 1
        End If

    If maxEntries% > 0 Then
        For a% = 0 To maxEntries - 1
            If Left$(myDirectory(a%).Name, 9) = "newbbs000" Then
                myDirectory(a%) = myDirectory(a% + 1)
                maxEntries% = maxEntries% - 1
                End If
            Next
        End If
    
    If b% = 1 Then Call SaveDialingDirectory
    Call display_directory

End Sub

Private Sub New_Button_Click()
    
    maxEntries% = maxEntries% + 1

    z = maxEntries% - 1

    myDirectory(z).Name = "newbbs000"
    myDirectory(z).PhoneOrAddress = ""
    myDirectory(z).TCPport = 504
    
    Call SaveDialingDirectory
    Call display_directory
    List1.ListIndex = z
    
    Call edit_button_click
End Sub

Private Sub Prefs_button_Click()
    Load Preferences
    Unload SelectBBS
End Sub

Private Sub SaveDialingDirectory()
    On Error Resume Next
    Open "dialing.dir" For Output As #1
    If Err = 0 Then
        If maxEntries > 0 Then
            For a% = 0 To maxEntries - 1
                Write #1, myDirectory(a%).Name, myDirectory(a%).PhoneOrAddress, myDirectory(a%).TCPport
                Next
            End If
        Close #1
        End If

End Sub


