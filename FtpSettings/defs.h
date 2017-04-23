#ifndef _FTPDRIVE_DEFS_H_
#define _FTPDRIVE_DEFS_H_
#define FTPS_FROMLOCAL              0x01
#define FTPS_FROMREMOTE             0x02
#define FTPS_USESTOREDMODE          0x04
#define FTPS_USESTOREDCREDENTIALS   0x08
#define FTPS_DENIEDAPPS             0x10
#define FTPS_DEFPASSIVE             0x20
#define FTPS_HOOKSMBOPS             0x40

#define FTPS_ADV_DIRCACHE           0x01
#define FTPS_ADV_FILECACHE          0x02
#define FTPS_ADV_USEGUARDPAGES      0x04
#define FTPS_ADV_FORWARDBYPASS      0x08
#define FTPS_ADV_IOERRCTRLFTP       0x10
#define FTPS_ADV_INJECTONTHEFLY     0x20
#define FTPS_ADV_MULTIDESKSUPPORT   0x40

#define default_serv_host           TEXT("127.0.0.1")
#define default_serv_port           1215
#define default_serv_user           TEXT("Anonymous")
#define default_serv_pass           TEXT("")
#define default_serv_refresh        60
#define default_set_flags           (FTPS_FROMLOCAL|FTPS_DEFPASSIVE|FTPS_USESTOREDCREDENTIALS|FTPS_USESTOREDMODE)
#define default_app_list            TEXT("*\r\n")//"explorer.exe\r\nkillcopy.exe\r\nwinamp.exe\r\nfoobar.exe\r\nmplayer2.exe\r\nwmplayer.exe\r\nfar.exe\r\nbsplayer.exe\r\nla.exe\"
#define default_ftp_expire          20
#define default_drv_letter          TCHAR('Z')
#define default_interface_lng       TEXT("English")

#define default_adv_flags              (FTPS_ADV_DIRCACHE|FTPS_ADV_FILECACHE|FTPS_ADV_USEGUARDPAGES|FTPS_ADV_FORWARDBYPASS|FTPS_ADV_IOERRCTRLFTP)
#define default_cache_dir_expire       300
#define default_cache_file_expire      300
#define default_cache_max_file_size    128
#define default_pre_seek_delay         100
#define default_idle_timeout           30
#define default_ftp_replies_timeout    10
#define default_ftp_data_timeout       30
#define default_err_maxretries         100
#define default_err_retrydelay         50
#define default_hk_unfreeze            0x0655

#endif