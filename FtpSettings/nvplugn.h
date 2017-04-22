//************************************
//*NetView plug-ins header file v2.93*
//*     (c)Killer{R} 02.07.2005 Rev A*
//************************************
#ifndef nvplugh
#define nvplugh
#pragma pack(push,1)

//------------------------------------------------
//NMPN_NETSEND
//Use this message to send net send messages via NetView NetMessenger
//wParam = 0
//lParam points to initialized NVNETSEND structure
//NVNETSEND::flags can be a any combination of following flags:
#define NVNETSEND_WAITCOMPLETE 0x01//if specified call will wait until message will be send or failed. Returns error code or 0 on success.
                                   //if not specified call immidiately returns and it is not possible to get result code

#define NVNETSEND_WAITPENDING  0x02//if specified call will wait for completion of any pending sends
                                   //if not specified call returns with code -1 if thereis pending send

#define NVNETSEND_SHOWUI       0x04//if specified call will set from, to and message fields in NetMessenger window and show it
                                   //without send. If NVNETSEND_SHOWUI specified then NVNETSEND_WAITCOMPLETE flag ignored


//------------------------------------------------
//NMPN_NVEXTEXEC
//Use this message to exec external programs from plug-ins instead
//of calling CreateProcess/ShellExecute/WinExec whenever possible
//When NetView runs as service any processes that created by plug-in
//directly with CreateProcess/ShellExecute/WinExec will run under SYSTEM account
//calling this functions allow run processes under currently logged-on user
//if thereis no interactively logged-on users NMPN_NVEXTEXEC fails and return 0xffffffff
//Also it is possible to simple check is there logged-on user present and is it privileged 
//user (that is Administrator or any other who has write permissions to NetView.ini file)
///wParam=0, lParam=0 - to simple get logged-on user status
///wParam=1, lParam points to NULL-terminated char array - exec program with WinExec under logged user
///wParam=2, lParam points to NULL-terminated char array - exec program/document with ShellExecute under logged user
///Return 0 if no logged user present
///Return 1 - if logged user is present but it is not privileged
///Return 2 - if logged user is present and it is privileged
///Return 0xffffffff - if program execution failed due to any other cause

//------------------------------------------------
//NMPN_IMAGES
//Use this message to obtain any NetView interface-specified
//images such as indexed menus images loaded from images.bmp
//and shared and ftp resource name assotiated images
//HICON's returned are owned by NetView so plug-in shouldn't
//destroy it after use
#define NVIMAGES_GETRESIMG    0x01//Retrieves HICON associated to given resource name in NetView settings
                                  ///lParam - points to NULL-terminated resourcename string
                                  ///returns HICON as result or NULL if no icon associated

#define NVIMAGES_GETRESIMGCH  0x02//Same as NVIMAGES_GETRESIMG but "cached resource" image will be returned

#define NVIMAGES_GETNVIMG     0x03//Retrieves HICON of internal NetView image associated to given index
                                  ///Icons are images extracted from images.bmp on startup
                                  ///lParam specifies icon index as zero-based image number in images.bmp
                                  ///returns HICON as result

//------------------------------------------------
//NMNP_HOSTMSG
//Sent by NetView to plug-ins and scripts when
//user double clicked on the host with specified
//and enabled "hostmsg://NNN" command on advanced page
//of host's edition dialog. wParam - user specified
//number NNN. lParam - host's id.


//------------------------------------------------
//NMPN_TRYAUTH
//Used to authorize self on local computer using stored in metavariables user and password or using
//random-generated username
//if wParam = 0:
///try authorize host with stored username and password in variables
///"user" and "pass". If no user/pass stored do nothing
//if wParam = 1:
///try authorize host with stored username and password in variables
///"user" and "pass". If no user/pass stored try random username
//lParam - host's ID

//------------------------------------------------
//NMPN_GETRESLIST
//Used to get NETBIOS or FTP resourcelist of specified hostname
//NETBIOS and FTP settings determined by general NetView's settings
//and host's individual settings
//Flags for wParam:
#define NVRESLIST_FTP     0 //get FTP resources (if enabled by settings)
#define NVRESLIST_NETBIOS 1 //get NETBIOS resources (if enabled by settings)
#define NVRESLIST_USEID   2 //bitmask flag, can be used with one of above, tells
                            ///NetView to use NVRESLIST::host.id
                            ///(if not specified NVRESLIST::host.name used)

//lParam points to NVRESLIST structure. If thereis not enough buffer length
//to place result message returns 2 and set buflen field to neccessary length.
//In case of success returns 1. By the way if hosts has no resources or if network
//error occured message returns 1, but resourcelist contains only character '\1'
//If other error occured returns 0



//------------------------------------------------
//NMPN_CALLBACK
//Used to set plugins callback functions to handle event of recheck host with
//disabled default rechecks or to make some work on every host's SETSTATE event
//lParam points to such callback function :
//LRESULT CALLBACK NVCallBackProc(DWORD EventCode, DWORD ObjId, void *Reserved)
//EventCode - event code: NVCB_CUSTOM_RECHECK or NVCB_PRE_SETSTATE (see below)
//ObjId     - host's id 
//function must return 1
//Flags in wParam:
#define NVCB_CUSTOM_RECHECK       0x0001//to enable notifies on recheck of hosts
                                        ///with disabled default recheck mechanism
#define NVCB_PRE_SETSTATE         0x0002//to enable notifies just before hosts setstate
#define NVCB_REMOVECALLBACK       0x1000//use to remove callback function or disable some events


//------------------------------------------------
//NMPN_NVCTL
//wParam=0;lParam=0 - shows and activates main NetView window
//wParam=1;lParam points to MSG structure. Message in MSG will
// be broadcasted to all currently loaded plugins

//------------------------------------------------
//NMPN_PLUGINEX -used to connect external process as plugin
//wParam
//0 to query NetView info by external process
//1 to remove thread from NetView's plugins list
//2 to add thread to NetView's plugins list
//lParam points to NVPLUGININFO structure
//process must allocate this structure
//process must allocate NVPLUGINGOINFO structure and set pointer to it in NVPLUGININFO.goinfo
//process nust allocate and free all structrures in address space of NetView, by using
//VirtualAllocEx/VirtualFreeEx, read and write memory by ReadProcessMemory/WriteProcessMemory
//process must correctly remove itself from plugins list on exiting and when received WM_QUIT
//from NetView to avoid blocking NetView on exit

//------------------------------------------------
//NetView sets this flags in NMNP_ALERT as wParam
//lParam depends on alert type
#define NVALERT_NETWATCHER   0x00000001//NetWatcher detected user connection, lParam - host id or NULL
#define NVALERT_ALARMHOST    0x00000002//Alarm host in host list goes up or down, lParam - host id or NULL
#define NVALERT_TERMINAL     0x00000003//Terminal accepted client in server mode, lParam - host id or NULL
#define NVALERT_REDIRECTOR   0x00000004//Redirector accepted client, lParam - host id or NULL
#define NVALERT_IPLOGGER     0x00000005//IP logger detected activity or flood atack,
#define NVALERT_NETSEARCHER  0x00000006//NetSearcher event send on end searching
#define NVALERT_RESSCANER    0x00000007//Resources Searcher event send on end of scan

#define NVALERTMASK_CANCEL   0x00040000//bitmask. If set continious event has been finished.(Alarmhost goes down, NetWatcher - user disconnected, Terminal, Redirector - client disconnected, IP logger - no portlistener activity, NVALERT_ALARMHOST - user clicks on alarmed host)

#define NVALERTMASK_NWBLACK  0x00010000//NVALERT_NETWATCHER
#define NVALERTMASK_NWWHITE  0x00020000//masks
#define NVALERTMASK_NWSKIP   0x00080000//

#define NVALERTMASK_ILLIST   0x00010000///////
#define NVALERTMASK_ILICMP   0x00020000//NVALERT_IPLOGGER
#define NVALERTMASK_ILSYN    0x00080000//masks
#define NVALERTMASK_ILUDP    0x00100000///////

#define NVALERTMASK_HOSTUP   0x00010000//NVALERT_ALARMHOST
#define NVALERTMASK_HOSTDOWN 0x00020000//masks

//------------------------------------------------
//use with NMPN_OBJECT
//lParam points to NVHOST, NVLIST, NVLINE or NVAREA structures(or even represents objects IDs)
//wParam:

#define NVOBJ_SELECTED       0x10000//Use with NVOBJ_GETHOST - to get list of selected hosts
                                    ///or NVOBJ_GETLIST to get currently active hostlist
                                    ///(id will be set to NULL if there is no active hostlist's window)
#define NVOBJ_FORCENEW       0x20000//Use with NVOBJ_HOSTBYTEXT and NVOBJ_LISTBYTEXT
#define NVOBJ_HOSTNOIP       0x40000//Use with NVOBJ_HOSTBYTEXT|NVOBJ_FORCENEW to add host
                                    //with empty IP preventing any resolving
                                    //you can add host with random name and empy IP then
                                    //make it static and set true name and IP
#define NVOBJ_GETHOST        0x00001//set id to NULL to get first host
                                    ///selected hosts will be returned if NVOBJ_SELECTED
#define NVOBJ_SETHOST        0x00002//set id to NULL to add new host
                                    ///Note that if exists host with same name or IP
                                    ///it will be rescanned and returned
#define NVOBJ_DELHOST        0x00004
#define NVOBJ_HOSTBYTEXT     0x00008//lp points to hostname or ip address. Message returns host id
                                    ///if NVOBJ_FORCENEW specified new host will be created

//
#define NVOBJ_GETLIST        0x00010//set id to zero to get first list
#define NVOBJ_SETLIST        0x00020//set id to zero to create new list.
                                    ///Note that if exists list with same name it will be returned
#define NVOBJ_DELLIST        0x00040//lParam represents list id
#define NVOBJ_LISTBYTEXT     0x00080//lParam points to mapname, can be used with NVOBJ_FORCENEW
                                    ///Message returns list id



#define NVOBJ_GETLINE        0x00100//Set index field in NVLINE structure.
                                    ///Message result equals total line count.
#define NVOBJ_SETLINE        0x00200//Set index field in NVLINE structure. Set Index to -1 to add new line.
                                    ///Message result equals total line count after. Set Nodes to -1 to allow
                                    ///user to add nodes manually (first line node will be placed on the last
                                    ///right-mouse click position). Note that if Nodes=-1  then line's hostlist
                                    ///will be activated and switched to Visual Map mode, if not in it
#define NVOBJ_DELLINE        0x00400//Set lParam to index of line to be deleted.
                                    ///Message result equals total line count after.


#define NVOBJ_GETAREA        0x00800//set id to zero to get first area
                                    //return id if success or 0 if error occured
#define NVOBJ_SETAREA        0x01000//set id to zero to create new area if there is no area with the same name on the same map
                                    ///Note that if exists area with same name on specified map it will be returned
                                    //return id if success or 0 or -1 if error occured
#define NVOBJ_DELAREA        0x02000//lParam represents area's id
                                    //return 1 if success or 0 if error occured


//------------------------------------------------


//use with NMPN_MENU
//wParam      MENUACTION_SET|MENUACTION_DEL|MENUACTION_SPLIT
//lParam      LPNVMENUINFO  |DWORD id
#define NVMENUACTION_SET   0//set id to zero to create new menu item and it's id will be returned
#define NVMENUACTION_DEL   1//deletes menu item
#define NVMENUACTION_SPLIT 2//split menu submenu items to fit in screen 
#define NVMENUFLAG_MAIN     0x01//Menu item will be appeared in Plug-ins submenu. This flag only used when creating non-submenu item.
#define NVMENUFLAG_CONTEXT  0x02//Menu item will be appeared in context menu. This flag only used when creating non-submenu item.
#define NVMENUFLAG_TRAY     0x04//Menu item will be appeared in tray icon popup menu. This flag only used when creating non-submenu item.
#define NVMENUFLAG_DISABLED 0x08//Menu item will be appeared as disabled
#define NVMENUFLAG_CHECKED  0x10//Menu item will be appeared as checked
#define NVMENUFLAG_SORTED   0x20//Menu item will be added at sorted position
//------------------------------------------------
//use with NMPN_METAVAR
//wParam = {NVMETAVAR_GET,NVMETAVAR_SET}
//lParam LPNVMETAVAR
#define NVMETAVAR_GET 0
//return 1 if success
//return 0 if fail
//return 2 if vallen too small to acomodate all data. Vallen on return equals data length
#define NVMETAVAR_SET 1
//return 1 if success
//return 0 if fail
#define NVMETAVAR_USEID 2
//Bit flags, is set then use "id" instead of "name" field of "host" union in NVMETAVAR structure

//------------------------------------------------
//NMPN_ACTION and NMNP_ACTION
//bitmask for wParam

//group flags
#define NVACTION_GETFROMNET  0x01//!use only when NVACTION_LIST specified
#define NVACTION_GETFROMFILE 0x02//!use only when NVACTION_LIST specified
                                 ///lParam contains (char *) - path to LMHOSTS
                                 ///to import or NULL - default will be used
#define NVACTION_RECHECK     0x08//if NVACTION_LIST not specified
                                 ///lParam contains ListView host id
                                 ///otherwise list id or NULL to recheck all lists
#define NVACTION_RETRACE     0x1000//if NVACTION_LIST not specified lParam contains list id or hostid

#define NVACTION_CLEARLIST   0x2000//lParam contains id of list to be deleted or NULL to clear entire list

#define NVACTION_LIST        0x20//if not set means single host action
///

//single flags
#define NVACTION_ACTIVATE    0x8000//Main netview window status changed lParam
                                   ///equals wParam in WM_ACTIVATE message sent
                                   ///to main window by system

#define NVACTION_MENUPOPUP   0x10000//NetView sends this message when plug-ins menu, tray menu or context menu popups
                                    //lParam equals NVMENUFLAG_MAIN, NVMENUFLAG_CONTEXT or NVMENUFLAG_TRAY
                                    //plugin can use this notification to dynamically enable/disable its menu items

#define NVACTION_OPEN        0x10//lParam contains host item id


#define NVACTION_OPENPATH    0x80000//lParam points to path to be opened

#define NVACTION_SAVELMHOSTS 0x100//use to save entire hostlist to specified file
                                  ///lParam contains pointer to file path\name
                                  ///or NULL - default LMHOSTS will be saved
                                  ///!All other flags should be not set

#define NVACTION_SETSTATE   0x400//lParam=0 Resets current state of NetView:
                                 ///lParam=hostid - redraws host on list

#define NVACTION_SETTINGS   0x800//lParam=0 - notifies plug-in that global settings has been changed by user

#define NVACTION_EDIT       0x20000 //lParam contains host id to edit

#define NVACTION_IMGEXPORT   0x40000//lParam contains exported list id
                                    ///this message can be sent by plugin to initiate export
                                    ///returns 1 if initiate export succed
                                    ///NetView sends this message when list has been exported

#define NVACTION_LOCKSAVE   0x100000//lParam = 1 - to increment saving hostlist locks counter
                                    ///lParam = 0 - to decrement saving hostlist locks counter
                                    ///lParam = 2 - do nothing, only return current locks counter
                                    ///NetView allow user to save hostlist if current locks counter is zero
                                    ///if autosave hostlist enabled and locks counter on NetView exit
                                    ///is not zero then hostlist will not be saved automatically on exit
                                    ///plug-in can increment locks counter before doing some integrity-critical
                                    ///task and decrement it after task is completed to prevent saving hostlist
                                    ///with incorrect data. For example NetView sets lock before loading hostlist
                                    ///from file and decrements counter after load finished

///

//followed bitmasks sets by NetView in wParam with NVACTION_OPEN and NVACTION_RECHECK
//only when NVACTION_LIST not set (single open/recheck host) lParam in this case
//represents hostitem id or NULL if no item associated
#define NVACTION_RESULTONLINE  0x40
#define NVACTION_RESULTOPENED  0x80
//-------------------------------------------------


//-------------------------------------------------
//-------------------------------------------------
//-----------------------Control codes definitions-
//plugin->netview main window main codes
//Use as 1st parameter in NVPLUGINGOINFO::nvcall (SendMessage available for compatibility))
#define NMPN_MENU          WM_USER+0x401
#define NMPN_METAVAR       WM_USER+0x402
#define NMPN_ACTION        WM_USER+0x403
#define NMPN_GETSHARELIST  WM_USER+0x404
#define NMPN_GETRESLIST    WM_USER+0x405
#define NMPN_TRYAUTH       WM_USER+0x406
#define NMPN_OBJECT        WM_USER+0x407
#define NMPN_IMAGES        WM_USER+0x408
#define NMPN_PLUGINEX      WM_USER+0x409
#define NMPN_CALLBACK      WM_USER+0x40A
#define NMPN_NVEXTEXEC     WM_USER+0x40B
#define NMPN_NETSEND       WM_USER+0x40C
#define NMPN_NVCTL         WM_USER+1215


//netview->plugin main thread
//NetView posts this messabes to plug-ins thread by PostThreadMessage)
#define NMNP_ACTION        WM_USER+0x403
#define NMNP_HOSTMSG       WM_USER+0x404
#define NMNP_ALERT         WM_USER+0x406
//--------------------------------------------structures
typedef struct _NVHOST{
                      char hostname[128];
                      char hostip[64];
                      DWORD id;         //set this value to zero to get first host.
                      DWORD nextid;
                      DWORD color;      //item color in list
                      DWORD res[4];
                      bool selected;    //item is selected in list
                      }NVHOST,*LPNVHOST;
typedef struct _NVLIST{
                      char name[128];
                      char theme[128];
                      char ltype[128];
                      char llength[128];
                      char lspeed[128];
                      char lnote[128];
                      int  w[32];
                      DWORD id;
                      DWORD nextid;
                      DWORD reserved[128];
                      }NVLIST,*LPNVLIST;
typedef struct _NVLINE{
                      int index;
                      int x[32];//nodes points
                      int y[32];///
                      int nodes;//nodes count
                      DWORD color;
                      int flags;
                      int width;
                      char ltype[128];
                      char llength[128];
                      char lspeed[128];
                      char lnote[128];
                      char mapname[128];
                      DWORD selnode;
                      DWORD reserved[127];
                      }NVLINE,*LPNVLINE;
typedef struct _NVAREA{
                      RECT bounds;
                      DWORD color;
                      char hint[256];
                      char name[256];
                      char mapname[128];
                      DWORD id;
                      DWORD nextid;
                      DWORD reserved[128];
                      }NVAREA,*LPNVAREA;

typedef struct _NVRESLIST{
                         union
                          {
                          char name[128];//hostname which resources list to retrieve
                          DWORD id;//if NVRESLIST_USEID specified
                          }host;
                         DWORD reserved[8];
                         int buflen;
                         char bufdata[];
                         }NVRESLIST,*LPNVRESLIST;


typedef struct _NVNETSEND{
                         char from[256];
                         char to[256];
                         DWORD flags;
                         int msglen;
                         char msg[];
                         }NVNETSEND;

typedef struct _NVMENUINFO{DWORD id;       //After NVMENUACTION_SET call with id=0 id
                                           //will be contain menu handle wich can be
                                           //used in subsequent calls NVMENUACTION_SET
                                           //or NVMENUACTION_DEL
                           DWORD msg;      //Message to send to plug-in's thread on click
                           DWORD tid;      //ThreadId to wich send message
                           HICON icon;     //HANDLE to icon or NULL
                           DWORD parentid; //id of the parent menu item or NULL.
                                           //If it is not NULL NVMENUFLAG_MAIN,NVMENUFLAG_CONTEXT,NVMENUFLAG_TRAY
                                           //doesn't matter when creating menu item
                           char text[64];  //item text
                           DWORD flags;    //see NVMENUFLAG_######
                           char reserved[256];//set to zero
                           }NVMENUINFO,*LPNVMENUINFO;
typedef struct _NVMETAVAR{
                          union
                           {
                           char name[128];//hostname which metavariable set or retrieve
                           DWORD id;//if
                           }host;
                          char varname[64];  //variable name. Cannot contain characters
                                             //'#', '$', #0, #13
                          int vallen;        //length of the variable data
                          char val[];        //variable data. Cannot contain characters
                                             //'#', '$', #0, #13
                          }NVMETAVAR,*LPNVMETAVAR;
                          
typedef struct _NVVERSION{unsigned char hi;unsigned char lo;}NVVERSION,*LPNVVERSION;

typedef DWORD (WINAPI *NVUPCALL)(//This function must be used instead of SendMessage since NetView v2.82
                                 DWORD ThreadId,//Plug-in's main thread (in what NVPLUGINGO executed)
                                 DWORD Msg,     //NetView call's code (NMPN_...)
                                 DWORD wParam,  //call-specific parameter, usually call flags
                                 DWORD lParam   //call-specific parameter, usually object ID or pointer
                                 );
typedef struct _NVPLUGINGOINFO{
                              HWND nvwnd;             //NetView's plug-in messages processing window hanle
                                                      //use as the 1st parameter in SendMessage
                              HWND nvmainwnd;         //NetView's main window hanle
                              char *inipath;          //path to NetView .ini file
                              char *exepath;          //path and name of NetView.exe file
                              NVUPCALL nvcall;        //SendMessage substitude for call NetView's APIs since NV2.82
                              char reserved[160];
                              }NVPLUGINGOINFO,*LPNVPLUGINGOINFO;
typedef struct _NVPLUGININFO{
                            NVVERSION nvver;//NetView sets this field to it's version
                            NVVERSION retver;//Plug-in should sets this field to it's version
                            char plname[64]; //plug-in name
                            char retstr[128];//plug-in description
                            LPNVPLUGINGOINFO goinfo; //used only with NMPN_PLUGINEX. External thread must allocate this structure
                            DWORD tid;               //used only with NMPN_PLUGINEX. External thread ID.
                            char reserved[244];      //set to zero
                            }NVPLUGININFO,*LPNVPLUGININFO;



//plugin exported functions--------------------

typedef DWORD (WINAPI *NVPLUGINGETINFO)(LPNVPLUGININFO);//export name NVPluginGetInfo

typedef DWORD (WINAPI *NVPLUGINGO)(LPNVPLUGINGOINFO);   //export name NVPluginGo

typedef DWORD (WINAPI *NVPLUGINCONFIG)(void *reserved); //export name NVPluginConfig
                                                        //plugin can don't export this
#pragma pack(pop)                                                        
#endif
