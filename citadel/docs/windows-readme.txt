README for Windows distribution
-------------------------------

Installation:

  To install Citadel/UX for Windows, run the citadel-X.XX.exe file.  It is
  recommended that you do not change the installation folder.  Unlike the
  Unix version, you do NOT need to run Citadel's setup utility before
  starting the server.  However, the system operator will be set to the
  first user who creates an account on the server, so you need to login
  immediately.

  Setup will fail if the Citadel server is running when you install/upgrade.
  You will need to login to your Citadel server and issue the <.A>ide
  <T>erminate <N>ow command to stop the server before upgrading Citadel.
  This will be fixed in a future release.

  At the present time you must restart your computer to start the Citadel
  server.  This will be fixed in a future release.

Removal:

  To remove Citadel/UX for Windows, go to Start > Control Panel > Add/Remove
  Programs, click Citadel, then click Add/Remove (or Change/Remove).  Your
  server data (users, messages, etc.) will NOT be removed.  To remove these
  files, delete the C:\Program Files\Citadel\data directory.

Known Issues in Citadel/UX for Windows:

* Calendaring support is not included.  Status:  TO_BE_FIXED

* Citadel may not run properly if the installation directory is changed from
  C:\Program Files\Citadel.  It is strongly recommended that you do not
  change the installation directory.  Status:  TO_BE_FIXED

* The "weekly" maintenance script does not run.  Since it is a Unix script,
  there is no way to run it in Windows.  The main effect you will see is that
  no server objects expire.  If you wish you can expire them manually.  To do
  this, enter the following server commands (using the @ key in the text
  client or the appropriate advanced command in WebCit):  Status:  TO_BE_FIXED

  EXPI users
  EXPI rooms
  EXPI messages
  EXPI visits
  EXPI usetable

* Sessions can be disconnected if you read the citadel-debug.txt using a
  Cygwin program while the server is running.  In addition, the file is
  written in Unix text format.  To view this file, use a Windows editor
  which understands Unix text files, such as WordPad.  (Notepad won't work.)
  Status:  RELNOTE
