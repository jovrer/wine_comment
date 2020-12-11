/*
 * Copyright 2004 Jacek Caban
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef EXDISPID_H_
#define EXDISPID_H_

#define DISPID_BEFORENAVIGATE        100
#define DISPID_NAVIGATECOMPLETE      101
#define DISPID_STATUSTEXTCHANGE      102
#define DISPID_QUIT                  103
#define DISPID_DOWNLOADCOMPLETE      104
#define DISPID_COMMANDSTATECHANGE    105
#define DISPID_DOWNLOADBEGIN         106
#define DISPID_NEWWINDOW             107
#define DISPID_PROGRESSCHANGE        108
#define DISPID_WINDOWMOVE            109
#define DISPID_WINDOWRESIZE          110
#define DISPID_WINDOWACTIVATE        111
#define DISPID_PROPERTYCHANGE        112
#define DISPID_TITLECHANGE           113
#define DISPID_TITLEICONCHANGE       114

#define DISPID_FRAMEBEFORENAVIGATE   200
#define DISPID_FRAMENAVIGATECOMPLETE 201

#define DISPID_FRAMENEWWINDOW        204

#define DISPID_BEFORENAVIGATE2       250
#define DISPID_NEWWINDOW2            251
#define DISPID_NAVIGATECOMPLETE2     252
#define DISPID_ONQUIT                253
#define DISPID_ONVISIBLE             254
#define DISPID_ONTOOLBAR             255
#define DISPID_ONMENUBAR             256
#define DISPID_ONSTATUSBAR           257
#define DISPID_ONFULLSCREEN          258
#define DISPID_DOCUMENTCOMPLETE      259
#define DISPID_ONTHEATERMODE         260
#define DISPID_ONADDRESSBAR          261
#define DISPID_WINDOWSETRESIZABLE    262
#define DISPID_WINDOWCLOSING         263
#define DISPID_WINDOWSETLEFT         264
#define DISPID_WINDOWSETTOP          265
#define DISPID_WINDOWSETWIDTH        266
#define DISPID_WINDOWSETHEIGHT       267
#define DISPID_CLIENTTOHOSTWINDOW    268
#define DISPID_SETSECURELOCKICON     269
#define DISPID_FILEDOWNLOAD          270
#define DISPID_NAVIGATEERROR         271
#define DISPID_PRIVACYIMPACTEDSTATECHANGE 272

#define DISPID_PRINTTEMPLATEINSTANTIATION 225
#define DISPID_PRINTTEMPLATETEARDOWN      226
#define DISPID_UPDATEPAGESTATUS           227

#define DISPID_WINDOWREGISTERED 200
#define DISPID_WINDOWREVOKED    201

#define DISPID_RESETFIRSTBOOTMODE    1
#define DISPID_RESETSAFEMODE         2
#define DISPID_REFRESHOFFLINEDESKTOP 3
#define DISPID_ADDFAVORITE           4
#define DISPID_ADDCHANNEL            5
#define DISPID_ADDDESKTOPCOMPONENT   6
#define DISPID_ISSUBSCRIBED          7
#define DISPID_NAVIGATEANDFIND       8
#define DISPID_IMPORTEXPORTFAVORITES 9
#define DISPID_AUTOCOMPLETESAVEFORM  10
#define DISPID_AUTOSCAN              11
#define DISPID_AUTOCOMPLETEATTACH    12
#define DISPID_SHOWBROWSERUI         13
#define DISPID_SHELLUIHELPERLAST     13

#define DISPID_ADVANCEERROR            10
#define DISPID_RETREATERROR            11
#define DISPID_CANADVANCEERROR         12
#define DISPID_CANRETREATERROR         13
#define DISPID_GETERRORLINE            14
#define DISPID_GETERRORCHAR            15
#define DISPID_GETERRORCODE            16
#define DISPID_GETERRORMSG             17
#define DISPID_GETERRORURL             18
#define DISPID_GETDETAILSSTATE         19
#define DISPID_SETDETAILSSTATE         20
#define DISPID_GETPERERRSTATE          21
#define DISPID_SETPERERRSTATE          22
#define DISPID_GETALWAYSSHOWLOCKSTATE  23

#define DISPID_FAVSELECTIONCHANGE  1
#define DISPID_SELECTIONCHANGE     2
#define DISPID_DOUBLECLICK         3
#define DISPID_INITIALIZED         4

#define DISPID_MOVESELECTIONUP       1
#define DISPID_MOVESELECTIONDOWN     2
#define DISPID_RESETSORT             3
#define DISPID_NEWFOLDER             4
#define DISPID_SYNCHRONIZE           5
#define DISPID_IMPORT                6
#define DISPID_EXPORT                7
#define DISPID_INVOKECONTEXTMENU     8
#define DISPID_MOVESELECTIONTO       9
#define DISPID_SUBSCRIPTIONSENABLED  10
#define DISPID_CREATESUBSCRIPTION    11
#define DISPID_DELETESUBSCRIPTION    12
#define DISPID_SETROOT               13
#define DISPID_ENUMOPTIONS           14
#define DISPID_SELECTEDITEM          15
#define DISPID_ROOT                  16
#define DISPID_DEPTH                 17
#define DISPID_MODE                  18
#define DISPID_FLAGS                 19
#define DISPID_TVFLAGS               20
#define DISPID_NSCOLUMNS             21
#define DISPID_COUNTVIEWTYPES        22
#define DISPID_SETVIEWTYPE           23
#define DISPID_SELECTEDITEMS         24
#define DISPID_EXPAND                25
#define DISPID_UNSELECTALL           26

#endif /* EXDISPID_H_ */
