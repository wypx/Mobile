/**************************************************************************
 *
 * Copyright (c) 2017-2021, luotang.me <wypx520@gmail.com>, China.
 * All rights reserved.
 *
 * Distributed under the terms of the GNU General Public License v2.
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 *
 **************************************************************************/
#ifndef MOBILE_SRC_MOBILE_H_
#define MOBILE_SRC_MOBILE_H_

#include <base/noncopyable.h>
#include <Client/AgentClient.h>
#include <event/event_loop.h>
#include <event/event_stack.h>
#include <base/gcc_attr.h>
#include <base/mem_pool.h>
#include <base/os.h>
#include <base/logging.h>

#include <gflags/gflags.h>

#if 0
#include <brpc/server.h>
#endif

#include <list>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "Errno.h"
#include "Mobile.pb.h"

using namespace MSF;

namespace Mobile {

#define MODEM_TTY_USB_PREFIX "/dev/ttyUSB%d"

typedef enum {
  CME_ERROR_NON_CME = -1,
  CME_SUCCESS = 0,
  CME_SIM_NOT_INSERTED = 10
} ATCmeError;

enum EhrpdStatus {
  EHRPD_CLOSE = 0,
  EHRPD_OPEN = 1,
};

enum ChipErrLevel {
  CHIP_ERROR_NULL = 0,
  CHIP_ERROR_NUMERIC,
  CHIP_ERROR_LONG,
};

enum PdpStatus {
  PDP_DEACTIVE,
  PDP_ACTIVE,
};

enum TTYType {
  TTY_USB,
  TTY_ACM,
};

enum DialType {
  DIAL_AUTO_PERSIST = 0, /* Always dial to keep online */
  DIAL_AUTO_PLANS = 1,   /* Dial as scheduled */
  DIAL_MANUALLY = 2,     /* Dial manual when test*/
};

enum AuthType {
  AUTH_NONE,
  AUTH_PAP,
  AUTH_CHAP,
  AUTH_PAP_CHAP,
};

/**
 * odede:
 * @MODMODE_NONE: None.
 * @MODMODE_CS: CSD, GSM, and other circuit-switched technologies.
 * @MODMODE_2G: GPRS, EDGE.
 * @MODMODE_3G: UMTS, HSxPA.
 * @MODMODE_4G: LTE.
 * @MODMODE_ANY: Any mode can be used (only this value allowed for POTS modems).
 *
 * Bitfield to indicate which access modes are supported, allowed or
 * preferred in a given device.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=modmode >*/
              MODMODE_NONE = 0,
              MODMODE_CS = 1 << 0,
              MODMODE_2G = 1 << 1,
              MODMODE_3G = 1 << 2,
              MODMODE_4G = 1 << 3,
              MODMODE_5G = 1 << 4,
              MODMODE_ANY = 0xFFFFFFFF} Modede;

enum NetWorkType {
  NETWORK_2G,
  NETWORK_3G,
  NETWORK_4G,
  NETWORK_5G
};

/**
 * odemPortType:
 * @MODPORT_TYPE_UNKNOWN: Unknown.
 * @MODPORT_TYPE_NET: Net port.
 * @MODPORT_TYPE_AT: AT port.
 * @MODPORT_TYPE_QCDM: QCDM port.
 * @MODPORT_TYPE_GPS: GPS port.
 * @MODPORT_TYPE_QMI: QMI port.
 * @MODPORT_TYPE_MBIM: MBIM port.
 * @MODPORT_TYPE_AUDIO: Audio port. Since 1.12.
 *
 * Type of modem port.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=modport_type >*/
              MODPORT_TYPE_UNKNOWN = 1,
              MODPORT_TYPE_NET = 2,
              MODPORT_TYPE_AT = 3,
              MODPORT_TYPE_QCDM = 4,
              MODPORT_TYPE_GPS = 5,
              MODPORT_TYPE_QMI = 6,
              MODPORT_TYPE_MBIM = 7,
              MODPORT_TYPE_AUDIO = 8, } ModemPortType;

/**
 * earerIpMethod:
 * @BEARER_IP_METHOD_UNKNOWN: Unknown method.
 * @BEARER_IP_METHOD_PPP: Use PPP to get IP addresses and DNS information.
 * For IPv6, use PPP to retrieve the 64-bit Interface Identifier, use the IID to
 * construct an IPv6 link-local address by following RFC 5072, and then run
 * DHCP over the PPP link to retrieve DNS settings.
 * @BEARER_IP_METHOD_STATIC: Use the provided static IP configuration given
 * by the modem to configure the IP data interface.  Note that DNS servers may
 * not be provided by the network or modem firmware.
 * @BEARER_IP_METHOD_DHCP: Begin DHCP or IPv6 SLAAC on the data interface to
 * obtain any necessary IP configuration details that are not already provided
 * by the IP configuration.  For IPv4 bearers DHCP should be used.  For IPv6
 * bearers SLAAC should be used, and the IP configuration may already contain
 * a link-local address that should be assigned to the interface before SLAAC
 * is started to obtain the rest of the configuration.
 *
 * Type of IP method configuration to be used in a given Bearer.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=bearer_ip_method >*/
              BEARER_IP_METHOD_UNKNOWN = 0,
              BEARER_IP_METHOD_PPP = 1,
              BEARER_IP_METHOD_STATIC = 2,
              BEARER_IP_METHOD_DHCP = 3, } BearerIpMethod;

/**
 * earerIpFamily:
 * @BEARER_IP_FAMILY_NONE: None or unknown.
 * @BEARER_IP_FAMILY_IPV4: IPv4.
 * @BEARER_IP_FAMILY_IPV6: IPv6.
 * @BEARER_IP_FAMILY_IPV4V6: IPv4 and IPv6.
 * @BEARER_IP_FAMILY_ANY: Mask specifying all IP families.
 *
 * Type of IP family to be used in a given Bearer.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=bearer_ip_family >*/
              BEARER_IP_FAMILY_NONE = 0,
              BEARER_IP_FAMILY_IPV4 = 1 << 0,
              BEARER_IP_FAMILY_IPV6 = 1 << 1,
              BEARER_IP_FAMILY_IPV4V6 = 1 << 2,
              BEARER_IP_FAMILY_ANY = 0xFFFFFFFF} BearerIpFamily;

/**
 * earerAllowedAuth:
 * @BEARER_ALLOWED_AUTH_UNKNOWN: Unknown.
 * @BEARER_ALLOWED_AUTH_NONE: None.
 * @BEARER_ALLOWED_AUTH_PAP: PAP.
 * @BEARER_ALLOWED_AUTH_CHAP: CHAP.
 * @BEARER_ALLOWED_AUTH_MSCHAP: MS-CHAP.
 * @BEARER_ALLOWED_AUTH_MSCHAPV2: MS-CHAP v2.
 * @BEARER_ALLOWED_AUTH_EAP: EAP.
 *
 * Allowed authentication methods when authenticating with the network.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=bearer_allowed_auth >*/
              BEARER_ALLOWED_AUTH_UNKNOWN = 0,
              /* bits 0..4 order match Ericsson device bitmap */
              BEARER_ALLOWED_AUTH_NONE = 1 << 0,
              BEARER_ALLOWED_AUTH_PAP = 1 << 1,
              BEARER_ALLOWED_AUTH_CHAP = 1 << 2,
              BEARER_ALLOWED_AUTH_MSCHAP = 1 << 3,
              BEARER_ALLOWED_AUTH_MSCHAPV2 = 1 << 4,
              BEARER_ALLOWED_AUTH_EAP = 1 << 5, } BearerAllowedAuth;

/**
 * odemCdmaRegistrationState:
 * @MODCDMA_REGISTRATION_STATE_UNKNOWN: Registration status is unknown or the
 * device is not registered.
 * @MODCDMA_REGISTRATION_STATE_REGISTERED: Registered, but roaming status is
 * unknown or cannot be provided by the device. The device may or may not be
 * roaming.
 * @MODCDMA_REGISTRATION_STATE_HOME: Currently registered on the home network.
 * @MODCDMA_REGISTRATION_STATE_ROAMING: Currently registered on a roaming
 * network.
 *
 * Registration state of a CDMA modem.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=modcdma_registration_state >*/
              MODCDMA_REGISTRATION_STATE_UNKNOWN = 0,
              MODCDMA_REGISTRATION_STATE_REGISTERED = 1,
              MODCDMA_REGISTRATION_STATE_HOME = 2,
              MODCDMA_REGISTRATION_STATE_ROAMING = 3,
} ModemCdmaRegistrationState;

/**
 * irmwareImageType:
 * @FIRMWARE_IMAGE_TYPE_UNKNOWN: Unknown firmware type.
 * @FIRMWARE_IMAGE_TYPE_GENERIC: Generic firmware image.
 * @FIRMWARE_IMAGE_TYPE_GOBI: Firmware image of Gobi devices.
 *
 * Type of firmware image.
 *
 * Since: 1.0
 */
typedef enum {/*< underscore_name=firmware_image_type >*/
              FIRMWARE_IMAGE_TYPE_UNKNOWN = 0,
              FIRMWARE_IMAGE_TYPE_GENERIC = 1,
              FIRMWARE_IMAGE_TYPE_GOBI = 2, } irmwareImageType;

enum DialStat {
  DIAL_INIT,
  DIAL_EXECUTING,
  DIAL_FAILED,
  DIAL_SUCCESS,
};

enum CallIdStatus {
  CALLER_ID_CLOSE = 0,
  CALLER_ID_OPEN = 1
};

enum RadioMode {
  /* minimum functionality, dsiable RF but reserve SIM card power supply,
   * previous mode must be offline */
  RADIO_LPM_MODE = 0,    /* Radio explictly powered off (eg CFUN=0) */
  RADIO_ONLINE_MODE = 1, /* full functionality (Default) */
  RADIO_OFFLINE_MODE =
      4,           /* disable phone both transmit and receive RF circuits */
  RADIO_FMODE = 5, /* Factory Test Mode */
  RADIO_RESTART_MODE = 6, /* Reset mode */
                          /* note:AT+CFUN=6 must be used after setting AT+CFUN=7
* If module in offline mode, must execute AT+CFUN=6 or restart module to
* online mode.*/
  RADIO_OFF_MODE =
      7, /* Offline Mode, Radio unavailable (eg, resetting or not booted) */
         /* note:If AT+CFUN=0/4 is used after setting AT+CFUN=7,
* module will restart to online mode */
};

typedef enum {
  CARD_STATE_ABSENT = 0,
  CARD_STATE_PRESENT = 1,
  CARD_STATE_ERROR = 2,
  CARD_STATE_RESTRICTED =
      3 /* card is present but not usable due to carrier restrictions.*/
} CardState;

struct LceStatusInfo {
  uint8_t lce_status; /* LCE service status:
                       * -1 = not supported;
                       * 0 = stopped;
                       * 1 = active.
                       */
  uint32_t actual_interval_ms; /* actual LCE reporting interval,
                                * meaningful only if LCEStatus = 1.*/
};

/* service status*/
enum ServiceStatus {
  SERVICE_NOT_FOUND = 0,
  SERVICE_RESTRICTED_SERVICE,
  SERVICE_VALID_SERVICE,
  SERVICE_RESTRICTED_REGIONAL_SERVICE,
  SERVICE_POWER_SAVING_HIBERATE_SERVICE,
};

/* service domain*/
enum ServiceDomain {
  /* No restriction at all including voice/SMS/USSD/SS/AV64 and packet data. */
  SERVICE_DOMAIN_NONE,
  /* Block all voice/SMS/USSD/SS/AV64 including emergency call due to
     restriction.*/
  SERVICE_DOMAIN_CS_NORMAL,
  /* Block packet data access due to restriction. */
  SERVICE_DOMAIN_PS_NORMAL,
  SERVICE_DOMAIN_CS_PS_ALL,
  /* Not registered, but MT is currently searching */
  SERVICE_DOMAIN_CS_PS_SEARCH,
};

/* Huawei for AT^SYSINFOEX, "^SYSCONFIG" (TD) | "^SYSCFG" (WCDMA):
 * system mode config parameters
 * LongSung for ^MODODREX or ^MODODR */
enum NETWORK_SERACH_TYPE {
  /* LongSung Modem */
  LS_UTMS_ONLY = 1,         /* Not Support Now */
  LS_AUTO = 2,              /* LTE > CDMA >HDR >TDS >WCDMA >GSM */
  LS_GONLY = 3,             /* GSM + CDMA ONLY */
  LS_GPREFERED = 4,         /* TDS Pre (TDS > GSM > LTE >WCDMA >HDR >CDMA) */
  LS_LTE_ONLY = 5,          /* LTE ONLY */
  LS_TDSCDMA_ONLY = 6,      /* TDSCDMA ONLY */
  LS_TDSCDMA_WCDMA = 7,     /* TDCMDA_WCDMA */
  LS_TDSCDMA_GWCDMA = 8,    /* TDCMDA_GWCDMA */
  LS_TDSCDMA_WCDMA_LTE = 9, /* TDCMDA_WCDMA_LTE */
  LS_HDR_ONLY = 10,         /* HDR ONLY */
  LS_LTE_PREFERED = 11,     /* LTE Pre (LTE > HDR >CDMA >TDS >WCDMA >GSM) */
  LS_HDR_PREFERED = 12,     /* HDR Pre (HDR >CDMA >LTE >TDS >WCDMA > GSM)*/
  LS_HDR_LTE = 13,          /* HDR and LTE */
  LS_HDR_CDMA = 14,         /* CDMA and HDR*/
  LS_CDMA_ONLY = 15,        /* CDMA and HDR*/

  /* Huawei Modem */
  HW_AUTO = 2,   /* Automatic */
  HW_CDMA = 3,   /* CDMA (not supported currently) */
  HW_HDR = 4,    /* HDR (not supported currently) */
  HW_HYBRID = 5, /* CDMA/HDR HYBRID (not supported currently) */
  HW_CDMA1X = 11,
  HW_EVDO = 12,
  HW_GSM = 13,
  HW_WCDMA = 14,
  HW_TDSCDMA = 15,
  HW_NOCHANGE = 16,
  HW_TDLTE = 17,
  HW_FDDLTE = 18,
};

/**
 * odemAccessTechnology:
 * @MODACCESS_TECHNOLOGY_UNKNOWN: The access technology used is unknown.
 * @MODACCESS_TECHNOLOGY_POTS: Analog wireline telephone.
 * @MODACCESS_TECHNOLOGY_GSM: GSM.
 * @MODACCESS_TECHNOLOGY_GCOMPACT: Compact GSM.
 * @MODACCESS_TECHNOLOGY_GPRS: GPRS.
 * @MODACCESS_TECHNOLOGY_EDGE: EDGE (ETSI 27.007: "GSM w/EGPRS").
 * @MODACCESS_TECHNOLOGY_UMTS: UMTS (ETSI 27.007: "UTRAN").
 * @MODACCESS_TECHNOLOGY_HSDPA: HSDPA (ETSI 27.007: "UTRAN w/HSDPA").
 * @MODACCESS_TECHNOLOGY_HSUPA: HSUPA (ETSI 27.007: "UTRAN w/HSUPA").
 * @MODACCESS_TECHNOLOGY_HSPA: HSPA (ETSI 27.007: "UTRAN w/HSDPA and HSUPA").
 * @MODACCESS_TECHNOLOGY_HSPA_PLUS: HSPA+ (ETSI 27.007: "UTRAN w/HSPA+").
 * @MODACCESS_TECHNOLOGY_1XRTT: CDMA2000 1xRTT.
 * @MODACCESS_TECHNOLOGY_EVDO0: CDMA2000 EVDO revision 0.
 * @MODACCESS_TECHNOLOGY_EVDOA: CDMA2000 EVDO revision A.
 * @MODACCESS_TECHNOLOGY_EVDOB: CDMA2000 EVDO revision B.
 * @MODACCESS_TECHNOLOGY_LTE: LTE (ETSI 27.007: "E-UTRAN")
 * @MODACCESS_TECHNOLOGY_ANY: Mask specifying all access technologies.
 *
 * Describes various access technologies that a device uses when registered with
 * or connected to a network.
 *
 * Since: 1.0
 */
typedef enum {
  RADIO_TECH_UNKNOWN = 0,
  RADIO_TECH_GPRS = 1,
  RADIO_TECH_EDGE = 2,
  RADIO_TECH_UMTS = 3,
  RADIO_TECH_IS95A = 4,
  RADIO_TECH_IS95B = 5,
  RADIO_TECH_1XRTT = 6,
  RADIO_TECH_EVDO_0 = 7,
  RADIO_TECH_EVDO_A = 8,
  RADIO_TECH_HSDPA = 9,
  RADIO_TECH_HSUPA = 10,
  RADIO_TECH_HSPA = 11,
  RADIO_TECH_EVDO_B = 12,
  RADIO_TECH_EHRPD = 13,
  RADIO_TECH_LTE = 14,
  RADIO_TECH_HSPAP = 15, /* HSPA+ */
  RADIO_TECH_GSM = 16,   /* Only supports voice */
  RADIO_TECH_TD_SCDMA = 17,
  RADIO_TECH_IWLAN = 18,
  RADIO_TECH_LTE_CA = 19
} RadioTechnology;

typedef enum {
  RADIO_TECH_3GPP = 1, /* 3GPP Technologies - GSM, WCDMA */
  RADIO_TECH_3GPP2 = 2 /* 3GPP2 Technologies - CDMA */
} RadioTechnologyFamily;

typedef enum {
  BAND_MODE_UNSPECIFIED =
      0,               //"unspecified" (selected by baseband automatically)
  BAND_MODE_EURO = 1,  //"EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
  BAND_MODE_USA =
      2,  //"US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
  BAND_MODE_JPN = 3,  //"JPN band" (WCDMA-800 / WCDMA-IMT-2000)
  BAND_MODE_AUS =
      4,  //"AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
  BAND_MODE_AUS_2 = 5,      //"AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
  BAND_MODE_CELL_800 = 6,   //"Cellular" (800-MHz Band)
  BAND_MODE_PCS = 7,        //"PCS" (1900-MHz Band)
  BAND_MODE_JTACS = 8,      //"Band Class 3" (JTACS Band)
  BAND_MODE_KOREA_PCS = 9,  //"Band Class 4" (Korean PCS Band)
  BAND_MODE_5_450M = 10,    //"Band Class 5" (450-MHz Band)
  BAND_MODE_IMT2000 = 11,   //"Band Class 6" (2-GMHz IMT2000 Band)
  BAND_MODE_7_702 = 12,     //"Band Class 7" (Upper 700-MHz Band)
  BAND_MODE_8_1800M = 13,   //"Band Class 8" (1800-MHz Band)
  BAND_MODE_9_900M = 14,    //"Band Class 9" (900-MHz Band)
  BAND_MODE_10_802 = 15,    //"Band Class 10" (Secondary 800-MHz Band)
  BAND_MODE_EURO_PAMR_400M = 16,  //"Band Class 11" (400-MHz European PAMR Band)
  BAND_MODE_AWS = 17,             //"Band Class 15" (AWS Band)
  BAND_MODE_USA_2500M = 18        //"Band Class 16" (US 2.5-GHz Band)
} RadioBandMode;

enum NetMode {
  MODE_NONE = 0,    /*NO SERVICE*/
  MODE_CDMA1X = 2,  /*CDMA*/
  MODE_GPRS = 3,    /*GSM/GPRS*/
  MODE_EVDO = 4,    /*HDR(EVDO)*/
  MODE_WCDMA = 5,   /*WCDMA*/
  MODE_GSWCDMA = 7, /*GSM/WCDMA*/
  MODE_HYBRID = 8,  /*HYBRID(CDMA and EVDO mixed)*/
  MODE_TDLTE = 9,   /*TDLTE*/
  MODE_FDDLTE = 10, /*FDDLTE(2)*/
  MODE_TDSCDMA = 15 /*TD-SCDMA*/
};

typedef enum {
  PREF_NET_TYPE_GWCDMA = 0,      /* GSM/WCDMA (WCDMA preferred) */
  PREF_NET_TYPE_GONLY = 1,       /* GSM only */
  PREF_NET_TYPE_WCDMA = 2,       /* WCDMA  */
  PREF_NET_TYPE_GWCDMA_AUTO = 3, /* GSM/WCDMA (auto mode, according to PRL) */
  PREF_NET_TYPE_CDMA_EVDO_AUTO =
      4,                       /* CDMA and EvDo (auto mode, according to PRL) */
  PREF_NET_TYPE_CDMA_ONLY = 5, /* CDMA only */
  PREF_NET_TYPE_EVDO_ONLY = 6, /* EvDo only */
  PREF_NET_TYPE_GWCDMA_CDMA_EVDO_AUTO =
      7,                           /* GSM/WCDMA, CDMA, and EvDo (auto mode) */
  PREF_NET_TYPE_LTE_CDMA_EVDO = 8, /* LTE, CDMA and EvDo */
  PREF_NET_TYPE_LTE_GWCDMA = 9,    /* LTE, GSM/WCDMA */
  PREF_NET_TYPE_LTE_CMDA_EVDO_GWCDMA = 10, /* LTE, CDMA, EvDo, GSM/WCDMA */
  PREF_NET_TYPE_LTE_ONLY = 11,             /* LTE only */
  PREF_NET_TYPE_LTE_WCDMA = 12,            /* LTE/WCDMA */
  PREF_NET_TYPE_TD_SCDMA_ONLY = 13,        /* TD-SCDMA only */
  PREF_NET_TYPE_TD_SCDMA_WCDMA = 14,       /* TD-SCDMA and WCDMA */
  PREF_NET_TYPE_TD_SCDMA_LTE = 15,         /* TD-SCDMA and LTE */
  PREF_NET_TYPE_TD_SCDMA_GSM = 16,         /* TD-SCDMA and GSM */
  PREF_NET_TYPE_TD_SCDMA_GLTE = 17,        /* TD-SCDMA,GSM and LTE */
  PREF_NET_TYPE_TD_SCDMA_GWCDMA = 18,      /* TD-SCDMA, GSM/WCDMA */
  PREF_NET_TYPE_TD_SCDMA_WCDMA_LTE = 19,   /* TD-SCDMA, WCDMA and LTE */
  PREF_NET_TYPE_TD_SCDMA_GWCDMA_LTE = 20,  /* TD-SCDMA, GSM/WCDMA and LTE */
  PREF_NET_TYPE_TD_SCDMA_GWCDMA_CDMA_EVDO_AUTO =
      21, /* TD-SCDMA, GSM/WCDMA, CDMA and EvDo */
  PREF_NET_TYPE_TD_SCDMA_LTE_CDMA_EVDO_GWCDMA =
      22 /* TD-SCDMA, LTE, CDMA, EvDo GSM/WCDMA */
} PreferredNetworkType;

typedef enum {
  CALL_STATE_ACTIVE = 0,
  CALL_STATE_HOLDING = 1,
  CALL_STATE_DIALING = 2,  /* MO call only */
  CALL_STATE_ALERTING = 3, /* MO call only */
  CALL_STATE_INCOMING = 4, /* MT call only */
  CALL_STATE_WAITING = 5   /* MT call only */
} CallState;

struct ApnItem {
  int cid_;    /* Context ID, uniquely identifies this call */
  int active_; /* 0=inactive, 1=active/physical link down, 2=active/physical
                 link    up */
  char *type_; /* One of the PDP_type values in TS 27.007 section 10.1.1.
                 For example, "IP", "IPV6", "IPV4V6", or "PPP". */
  char *ifname_;  /* The network interface name */
  char *apn_;     /* ignored */
  char *address_; /* An address, e.g., "192.0.1.3" or "2001:db8::1". */
};

struct MobileConf {
  std::string version_;

  std::string log_level_;
  std::string log_dir_;
  std::string pid_path_;

  bool daemon_ = false;
  bool coredump_ = true;
  bool enable_dial_ = true;
  DialType dial_type_ = DIAL_AUTO_PERSIST;
  NetWorkType net_type_ = NETWORK_4G;
  AuthType auth_type_ = AUTH_PAP_CHAP;
  TTYType tty_type_ = TTY_USB;
  std::unordered_map<uint32_t, ApnItem>
      active_apns_; /* eg. "public.vpdn.hz" - cid */

  // plans
  std::string dial_num_;
  std::string dial_user_;
  std::string dial_pass_;

  bool enable_sms_ = false;
  bool enable_sms_alarm_ = false;
  bool enable_sms_control_ = false;
  /* 支持接收告警和控制命令的SIM卡号列表 */
  std::set<std::string> sms_white_list_;
  std::set<uint32_t> sms_alatype_;      /* 支持的告警类型 */
  std::set<uint32_t> sms_control_type_; /* 支持的控制类型 */

  std::vector<std::string> plugins_;

  bool agent_enable_ = false;
  AgentNetType agent_net_type_;
  std::string agent_ip_;
  uint16_t agent_port_;
  std::string agent_unix_server_;
  std::string agent_unix_client_;
  uint32_t agent_unix_mask_;
  bool agent_auth_chap_ = false;
  std::string agent_pack_type_;

  std::string version() const { return version_; }
  void set_version(const std::string &version) { version_ = version; }

  // void set_log_level(Logger::LogLevel level) { log_level_ = level; }
  std::string log_dir() const { return log_dir_; }
  void set_log_dir(const std::string &dir) { log_dir_ = dir; }
  void set_pid_path(const std::string &pid_path) { pid_path_ = pid_path; }

  bool daemon() const { return daemon_; }
  void set_daemon(bool daemon) { daemon_ = daemon; }
  bool coredump() const { return coredump_; }
  void set_coredump(bool coredump) { coredump_ = coredump; }

  bool enable_dial() const { return enable_dial_; }
  void set_enable_dial(bool enable) { enable_dial_ = enable; }

  DialType dial_type() const { return dial_type_; }
  void set_dial_type(DialType type) { dial_type_ = type; }
  NetWorkType net_type() const { return net_type_; }
  void set_dial_type(NetWorkType type) { net_type_ = type; }
  AuthType auth_type() const { return auth_type_; }
  void set_auth_type(AuthType type) { auth_type_ = type; }
  TTYType tty_type() const { return tty_type_; }
  void set_tty_type(TTYType type) { tty_type_ = type; }

  void AddApn(const ApnItem &apn) { active_apns_[apn.cid_] = std::move(apn); }

  bool DelApn(uint32_t cid) {
    auto iter = active_apns_.find(cid);
    if (iter != active_apns_.end()) {
      active_apns_.erase(cid);
      return true;
    }
    return false;
  }

  uint32_t ApnSize() const { return active_apns_.size(); }

  void DebugApn() {
    for (auto iter = active_apns_.begin(); iter != active_apns_.end(); ++iter) {
    }
  }

  std::string dial_num() const { return dial_num_; }
  void set_dial_num(const std::string &number) { dial_num_ = number; }
  std::string dial_user() const { return dial_user_; }
  void set_dial_user(const std::string &user) { dial_user_ = user; }
  std::string dial_pass() const { return dial_pass_; }
  void set_dial_pass(const std::string &pass) { dial_pass_ = pass; }

  bool enable_sms() const { return enable_sms_; }
  void set_enable_sms(bool enable_sms) { enable_sms_ = enable_sms; }
  bool enable_sms_alarm() const { return enable_sms_alarm_; }
  void set_enable_sms_alarm(bool enable_sms_alarm) {
    enable_sms_alarm_ = enable_sms_alarm;
  }
  bool enable_sms_control() const { return enable_sms_control_; }
  void set_enable_sms_control(bool enable_sms_control) {
    enable_sms_control_ = enable_sms_control;
  }

  void AddSMSWhiteList(const std::string &phone_num) {
    sms_white_list_.emplace(phone_num);
  }
  void DelSMSWhiteList(const std::string &phone_num) {
    sms_white_list_.erase(phone_num);
  }
  uint32_t SMSWhiteListSize() const { return sms_white_list_.size(); }
  void DebugSMSWhiteList() {}

  void AddSMSAlarmType(uint32_t type) { sms_alatype_.emplace(type); }
  void DelSMSAlarmType(uint32_t type) { sms_alatype_.erase(type); }
  uint32_t SMSAlarmTypeSize() const { return sms_alatype_.size(); }
  void DebugSMSAlarmType() {}

  void AddSMSControlType(uint32_t type) { sms_control_type_.emplace(type); }
  void DelSMSControlType(uint32_t type) { sms_control_type_.erase(type); }
  uint32_t SMSControlTypeSize() const { return sms_control_type_.size(); }
  void DebugSMSControlType() {}

  // void AddPluin(const std::string &name) { plugins_.push_back(name); }
  // void DelPluin(const std::string &name) { plugins_.erase(name); }
  uint32_t PluginSize() const { return plugins_.size(); }
  void DebugPlugins() {}

  AgentNetType agent_net_type() const { return agent_net_type_; }
  void set_agent_net_type(AgentNetType net_type) { agent_net_type_ = net_type; }

  const std::string &agent_ip() const { return agent_ip_; }
  void set_agent_ip(const std::string &ip) { agent_ip_ = ip; }
  uint16_t agent_port() const { return agent_port_; }
  void set_agent_port(uint16_t port) { agent_port_ = port; }
  const std::string &agent_unix_server() const { return agent_unix_server_; }
  void set_agent_unix_server(const std::string &server) {
    agent_unix_server_ = server;
  }
  const std::string &agent_unix_client() const { return agent_unix_client_; }
  void set_agent_unix_client(const std::string &client) {
    agent_unix_client_ = client;
  }
  uint32_t agent_unix_mask() const { return agent_unix_mask_; }
  void set_agent_unix_mask(uint32_t mask) { agent_unix_mask_ = mask; }
  bool agent_auth_chap() const { return agent_auth_chap_; }
  void set_agent_auth_chap(bool auth_chap) { agent_auth_chap_ = auth_chap; }
  const std::string &agent_pack_type() const { return agent_pack_type_; }
  void set_agent_pack_type(const std::string &pack_type) {
    agent_pack_type_ = pack_type;
  }
};

struct MobileState {
  bool upgrade_;

  // SIMStatus sim_status_;
  uint8_t sim_status_;
  uint8_t reg_status_;
  uint8_t serv_status_;
  uint8_t serv_domain_;

  uint8_t signal_;
  uint8_t search_mode_;
  NetMode netMode_;

  std::string ip_addr_;
  std::string netmask_;
  std::string gateway_;
  std::vector<std::string> dns_list_;
};

class ATChannel;
class ATCmdManager;
class SMSManager;
class Modem;

class MobileApp {
 public:
  enum ThreadIndex {
    THREAD_ATCMDLOOP = 0,
    THREAD_DILALOOP = 1,
    THREAD_STATLOOP = 2
  };

 public:
  MobileApp();
  ~MobileApp();

  bool LoadConfig();
  void Init(int argc, char **argv);
  int32_t Start();

 private:
  MobileConf config_;

  OsInfo os_;
  MemPool *pool_;
  EventStack *stack_;
  Modem *modem_;
  bool quit_;

#if 0
  static brpc::Server server_;

  virtual void GetMobileAPN(google::protobuf::RpcController *cntl_base,
                            const GetMobileAPNRequest *request,
                            GetMobileAPNResponse *response,
                            google::protobuf::Closure *done);
  bool RegisterService();
#endif
};
}  // namespace Mobile
#endif