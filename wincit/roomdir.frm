VERSION 4.00
Begin VB.Form RoomDirectory 
   Appearance      =   0  'Flat
   BackColor       =   &H00C0C0C0&
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "Directory of this room..."
   ClientHeight    =   6150
   ClientLeft      =   1485
   ClientTop       =   2865
   ClientWidth     =   8985
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
   Height          =   6555
   Left            =   1425
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MDIChild        =   -1  'True
   MinButton       =   0   'False
   ScaleHeight     =   6150
   ScaleWidth      =   8985
   Top             =   2520
   Width           =   9105
   Begin VB.CommandButton download_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&Download"
      Height          =   495
      Left            =   5880
      TabIndex        =   69
      Top             =   5640
      Width           =   1575
   End
   Begin VB.VScrollBar TheScroll 
      Height          =   5055
      Left            =   8640
      TabIndex        =   4
      Top             =   480
      Width           =   255
   End
   Begin VB.CommandButton ok_button 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      Caption         =   "&OK"
      Height          =   492
      Left            =   7560
      TabIndex        =   0
      Top             =   5640
      Width           =   1332
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   20
      Left            =   3360
      TabIndex        =   1
      Top             =   5280
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   20
      Left            =   2280
      TabIndex        =   2
      Top             =   5280
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   20
      Left            =   120
      TabIndex        =   3
      Top             =   5280
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   19
      Left            =   3360
      TabIndex        =   7
      Top             =   5040
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   19
      Left            =   2280
      TabIndex        =   8
      Top             =   5040
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   19
      Left            =   120
      TabIndex        =   9
      Top             =   5040
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   18
      Left            =   3360
      TabIndex        =   10
      Top             =   4800
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   18
      Left            =   2280
      TabIndex        =   11
      Top             =   4800
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   18
      Left            =   120
      TabIndex        =   12
      Top             =   4800
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   17
      Left            =   3360
      TabIndex        =   13
      Top             =   4560
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   17
      Left            =   2280
      TabIndex        =   14
      Top             =   4560
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   17
      Left            =   120
      TabIndex        =   15
      Top             =   4560
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   16
      Left            =   3360
      TabIndex        =   16
      Top             =   4320
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   16
      Left            =   2280
      TabIndex        =   17
      Top             =   4320
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   16
      Left            =   120
      TabIndex        =   18
      Top             =   4320
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   15
      Left            =   3360
      TabIndex        =   19
      Top             =   4080
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   15
      Left            =   2280
      TabIndex        =   20
      Top             =   4080
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   15
      Left            =   120
      TabIndex        =   21
      Top             =   4080
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   14
      Left            =   3360
      TabIndex        =   22
      Top             =   3840
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   14
      Left            =   2280
      TabIndex        =   23
      Top             =   3840
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   14
      Left            =   120
      TabIndex        =   24
      Top             =   3840
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   13
      Left            =   3360
      TabIndex        =   25
      Top             =   3600
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   13
      Left            =   2280
      TabIndex        =   26
      Top             =   3600
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   13
      Left            =   120
      TabIndex        =   27
      Top             =   3600
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   12
      Left            =   3360
      TabIndex        =   28
      Top             =   3360
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   12
      Left            =   2280
      TabIndex        =   29
      Top             =   3360
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   12
      Left            =   120
      TabIndex        =   30
      Top             =   3360
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   11
      Left            =   3360
      TabIndex        =   31
      Top             =   3120
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   11
      Left            =   2280
      TabIndex        =   32
      Top             =   3120
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   11
      Left            =   120
      TabIndex        =   33
      Top             =   3120
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   10
      Left            =   3360
      TabIndex        =   34
      Top             =   2880
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   10
      Left            =   2280
      TabIndex        =   35
      Top             =   2880
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   10
      Left            =   120
      TabIndex        =   36
      Top             =   2880
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   9
      Left            =   3360
      TabIndex        =   37
      Top             =   2640
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   9
      Left            =   2280
      TabIndex        =   38
      Top             =   2640
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   9
      Left            =   120
      TabIndex        =   39
      Top             =   2640
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   8
      Left            =   3360
      TabIndex        =   42
      Top             =   2400
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   8
      Left            =   2280
      TabIndex        =   43
      Top             =   2400
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H00000000&
      Height          =   230
      Index           =   8
      Left            =   120
      TabIndex        =   44
      Top             =   2400
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   7
      Left            =   3360
      TabIndex        =   45
      Top             =   2160
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   7
      Left            =   2280
      TabIndex        =   46
      Top             =   2160
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   7
      Left            =   120
      TabIndex        =   47
      Top             =   2160
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   6
      Left            =   3360
      TabIndex        =   48
      Top             =   1920
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   6
      Left            =   2280
      TabIndex        =   49
      Top             =   1920
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   6
      Left            =   120
      TabIndex        =   50
      Top             =   1920
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   5
      Left            =   3360
      TabIndex        =   68
      Top             =   1680
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   5
      Left            =   2280
      TabIndex        =   67
      Top             =   1680
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   5
      Left            =   120
      TabIndex        =   66
      Top             =   1680
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   4
      Left            =   3360
      TabIndex        =   65
      Top             =   1440
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   4
      Left            =   2280
      TabIndex        =   64
      Top             =   1440
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   4
      Left            =   120
      TabIndex        =   63
      Top             =   1440
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   3
      Left            =   3360
      TabIndex        =   62
      Top             =   1200
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   3
      Left            =   2280
      TabIndex        =   61
      Top             =   1200
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   3
      Left            =   120
      TabIndex        =   60
      Top             =   1200
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   2
      Left            =   3360
      TabIndex        =   59
      Top             =   960
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   2
      Left            =   2280
      TabIndex        =   58
      Top             =   960
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   2
      Left            =   120
      TabIndex        =   57
      Top             =   960
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   1
      Left            =   3360
      TabIndex        =   56
      Top             =   720
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   1
      Left            =   2280
      TabIndex        =   55
      Top             =   720
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   1
      Left            =   120
      TabIndex        =   54
      Top             =   720
      Width           =   2175
   End
   Begin VB.Label filedesc 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   0
      Left            =   3360
      TabIndex        =   53
      Top             =   480
      Width           =   5175
   End
   Begin VB.Label filesize 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   0
      Left            =   2280
      TabIndex        =   52
      Top             =   480
      Width           =   1095
   End
   Begin VB.Label filename 
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      ForeColor       =   &H80000008&
      Height          =   230
      Index           =   0
      Left            =   120
      TabIndex        =   51
      Top             =   480
      Width           =   2175
   End
   Begin VB.Label MaxFilesDisplay 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Total number of files:"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   495
      Left            =   0
      TabIndex        =   41
      Top             =   5760
      Width           =   5055
   End
   Begin VB.Label Label3 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Description"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   375
      Left            =   4680
      TabIndex        =   5
      Top             =   0
      Width           =   3975
   End
   Begin VB.Label Label2 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Size"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   375
      Left            =   1680
      TabIndex        =   6
      Top             =   0
      Width           =   1935
   End
   Begin VB.Label Label1 
      Alignment       =   2  'Center
      Appearance      =   0  'Flat
      BackColor       =   &H80000005&
      BackStyle       =   0  'Transparent
      Caption         =   "Filename"
      BeginProperty Font 
         name            =   "MS Sans Serif"
         charset         =   0
         weight          =   700
         size            =   13.5
         underline       =   0   'False
         italic          =   0   'False
         strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H80000008&
      Height          =   375
      Left            =   120
      TabIndex        =   40
      Top             =   0
      Width           =   1575
   End
End
Attribute VB_Name = "RoomDirectory"
Attribute VB_Creatable = False
Attribute VB_Exposed = False
Dim FileSizeArr&(1024)
Dim FileNameArr$(1024)
Dim FileDescArr$(1024)
Dim MaxFiles%

Private Sub download_button_Click()

    If Len(DownLoadFileName$) > 0 Then
        Unload RoomDirectory
        Load Download
        End If

End Sub

Private Sub filename_Click(Index As Integer)
    For z% = 0 To 20
        filename(z%).ForeColor = &H0&
        Next z%
    filename(Index).ForeColor = &HFFFF&
    DownLoadFileName$ = filename(Index).Caption
    If Len(DownLoadFileName$) > 0 Then download_button.Enabled = True
End Sub

Private Sub Form_Load()


    Show
    RoomDirectory.WindowState = 0
    RoomDirectory.Top = Int((MainWin.Height - RoomDirectory.Height) / 3)
    RoomDirectory.Left = Int((MainWin.Width - RoomDirectory.Width) / 2)
    DoEvents

MaxFiles% = 0
DownLoadFileName$ = ""
download_button.Enabled = False

If begin_trans() = True Then
    
    serv_puts ("RDIR")
    a$ = serv_gets()
    
    If Left$(a$, 1) = "1" Then
        Do
            a$ = serv_gets()
            If (a$ <> "000") And MaxFiles% < 1024 Then
                FileSizeArr&(MaxFiles%) = Val(extract$(a$, 1))
                FileNameArr$(MaxFiles%) = extract$(a$, 0)
                FileDescArr$(MaxFiles%) = extract$(a$, 2)
                MaxFiles% = MaxFiles% + 1
                End If
            Loop Until a$ = "000"
        End If
    Call end_trans
    
    b% = MaxFiles% - 21
    If (b% < 0) Then b% = 0
    TheScroll.Max = b%

    MaxFilesDisplay.Caption = "Total number of files: " + Str$(MaxFiles%)
    Call LoadContents

    End If

End Sub

Private Sub LoadContents()

    For a% = 0 To 20

      If (TheScroll.Value + a%) <= (MaxFiles% - 1) Then

        If FileSizeArr&(TheScroll.Value + a%) <> 0 Then
            FileSize(a%).Caption = Str$(FileSizeArr&(TheScroll.Value + a%))
        Else
            FileSize(a%).Caption = " "
            End If
        filename(a%).Caption = FileNameArr$(TheScroll.Value + a%)
        FileDesc(a%).Caption = FileDescArr$(TheScroll.Value + a%)

      Else

        FileSize(a%).Caption = " "
        filename(a%).Caption = " "
        FileDesc(a%).Caption = " "

        End If

      Next a%


End Sub

Private Sub ok_button_Click()
    Unload RoomDirectory
    Load RoomPrompt
End Sub

Private Sub TheScroll_Change()

    Call LoadContents

End Sub

