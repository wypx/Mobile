/**************************************************************************
 *
 * Copyright (c) 2017, luotang.me <wypx520@gmail.com>, China.
 * All rights reserved.
 *
 * Distributed under the terms of the GNU General Public License v2.
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 *
 **************************************************************************/
#include "command.h"

#include <base/time_utils.h>
#include <event/event_loop.h>

#include "channel.h"
#include "modem.h"
#include "sms.h"
#include "token.h"

using namespace MSF;
using namespace mobile;

namespace mobile {

bool ATCommandManager::initModem() {
  int ret;
  ATResponse *rsp = nullptr;

  // _sendLoop->runInLoop(std::bind(&ATChannel::handShake, ch));

  /* note: we don't check errors here. Everything important will
   * be handled in onATTimeout and onATReaderClosed */

  /*  atchannel is tolerant of echo but it must */
  /*  have verbose result codes */
  send_command_("ATE1", nullptr);
  // queueATCmdInLoop("ATE0", nullptr);

  /*  No auto-answer */
  send_command_("ATS0=0", nullptr);

  /*  Extended errors */
  send_command_("AT+CMEE=1", nullptr);

  /*  set apn autometicly according to curent network operator */
  send_command_("AT+NVRW=1,50058,\"01\"", nullptr);

  // PDP_DEACTIVE
  // send_command_("AT+CGACT=1,1", nullptr);

  /* Auto:All band search*/
  send_command_("AT+BNDPRF=896,1272,131072", nullptr);

  /*  Network registration events */
  ret = send_command_("AT+CREG=2", &rsp);
  /* some handsets -- in tethered mode -- don't support CREG=2 */
  if (ret < 0 || rsp->success_ == 0) {
    send_command_("AT+CREG=1", nullptr);
  }

  free_resp_(rsp);

  /*  GPRS registration events */
  send_command_("AT+CGREG=1", nullptr);

  /*  Call Waiting notifications */
  send_command_("AT+CCWA=1", nullptr);

  /*  Alternating voice/data off */
  send_command_("AT+CMOD=0", nullptr);

  /*  Not muted */
  // send_command_("AT+CMUT=0", nullptr);

  /*  +CSSU unsolicited supp service notifications */
  send_command_("AT+CSSN=0,1", nullptr);

  /*  no connected line identification */
  send_command_("AT+COLP=0", nullptr);

  /*  HEX character set */
  // at_send_command("AT+CSCS=\"HEX\"", nullptr);

  /*  USSD unsolicited */
  send_command_("AT+CUSD=1", nullptr);

  /*  Enable +CGEV GPRS event notifications, but don't buffer */
  send_command_("AT+CGEREP=1,0", nullptr);

  /*  SMS PDU mode */
  send_command_("AT+CMGF=0", nullptr);

  return ret == 0 ? true : false;
}

bool ATCommandManager::setChipErrorLevel(const int level) {
  int ret;
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd) - 1, "AT+CMEE=%d", level);

  ret = send_command_(cmd, nullptr);
  if (ret == AT_SUCCESS) {
    return true;
  }
  return false;
}

bool ATCommandManager::resetModem() {
  send_command_("AT^RESET", nullptr);
  return true;
}

bool ATCommandManager::getModem(ModemMatch cb) {
  int ret;
  char *line;
  char *out;
  ATResponse *rsp = nullptr;

  /* use ATI or AT+CGMM or AT+LCTSW */
  ret = send_command_singleline_("ATI", "Model:", &rsp);
  if (ret < 0 || rsp->success_ == 0) {
    free_resp_(rsp);
    return false;
  }

  line = rsp->intermediates_->line_;

  ret = AtTokStart(&line);
  if (ret < 0) {
    free_resp_(rsp);
    return false;
  }

  ret = AtTokNextStr(&line, &out);
  if (ret < 0) {
    free_resp_(rsp);
    return false;
  }
  cb(out);

  free_resp_(rsp);
  return true;
}

/** returns 1 if on, 0 if off, and -1 on error */
RadioMode ATCommandManager::getRadioState() {
  ATResponse *rsp = nullptr;
  char ret;
  char *line = nullptr;

  ret = send_command_singleline_("AT+CFUN?", "+CFUN:", &rsp);
  if (ret < 0 || rsp->success_ == 0) {
    // assume radio is off
    free_resp_(rsp);
    return RADIO_LPM_MODE;
  }

  line = rsp->intermediates_->line_;
  ret = AtTokStart(&line);
  if (ret < 0) {
    free_resp_(rsp);
    return RADIO_LPM_MODE;
  }

  ret = AtTokNextBool(&line, &ret);
  if (ret < 0) {
    free_resp_(rsp);
    return RADIO_LPM_MODE;
  }

  free_resp_(rsp);
  return RadioMode(ret);
}

bool ATCommandManager::setRadioState(enum RadioMode cfun) {
  int rc;
  int count = 5;
  ATResponse *rsp = nullptr;
  enum RadioMode radioMode = RADIO_LPM_MODE;

  radioMode = getRadioState();
  if (radioMode == cfun) {
    return 0;
  }

  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd) - 1, "AT+CFUN=%d", cfun);
  while (count-- < 0) {
    rc = send_command_(cmd, &rsp);
    if (rc < 0 || rsp->success_ == 0) {
      continue;
    } else {
      radioMode = getRadioState();
      if (radioMode == cfun) break;
    }
  }
  free_resp_(rsp);
  return true;
}

int ATCommandManager::getSIMStatus() {
  int ret;
  char *cpinLine = nullptr;
  char *cpinResult = nullptr;
  ATResponse *rsp = nullptr;

  ret = send_command_singleline_("AT+CPIN?", "+CPIN:", &rsp);
  if (ret != 0) {
    ret = send_command_singleline_("AT+QCPIN?", "+QCPIN:", &rsp);
    if (ret != 0) {
      free_resp_(rsp);
      return SIM_NOT_READY;
    }
  }

  switch (get_cme_error_(rsp)) {
    case CME_SUCCESS:
      break;
    case CME_SIM_NOT_INSERTED:
      free_resp_(rsp);
      return SIM_ABSENT;
    default:
      free_resp_(rsp);
      return SIM_NOT_READY;
  }

  /* CPIN/QCPIN has succeeded, now look at the result */
  cpinLine = rsp->intermediates_->line_;
  ret = AtTokStart(&cpinLine);
  if (ret < 0) {
    free_resp_(rsp);
    return SIM_NOT_READY;
  }

  ret = AtTokNextStr(&cpinLine, &cpinResult);
  if (ret < 0) {
    free_resp_(rsp);
    return SIM_NOT_READY;
  }

  // AT_DBG("sim:%s\n", cpinResult);
  if (0 == strcmp(cpinResult, "SIM PIN")) {
    free_resp_(rsp);
    return SIM_PIN;
  } else if (0 == strcmp(cpinResult, "SIM PUK")) {
    free_resp_(rsp);
    return SIM_PUK;
  } else if (0 == strcmp(cpinResult, "PH-NET PIN")) {
    free_resp_(rsp);
    return SIM_NETWORK_PERSONALIZATION;
  } else if (0 != strcmp(cpinResult, "READY")) {
    /* we're treating unsupported lock types as "sim absent" */
    free_resp_(rsp);
    return SIM_ABSENT;
  }

  free_resp_(rsp);
  return SIM_READY;
}

int ATCommandManager::getSIMLock() {
  int ret = send_command_("AT+CLCK=\"SC\",2", nullptr);
  if (ret != 0) {
    /*Telecom*/
    ret = send_command_("AT+QCLCK=\"SC\",2", nullptr);
    if (ret != 0) {
      return -1;
    }
  }
  return 0;
}

int ATCommandManager::setSIMUnlockPinCode(const std::string &pincode) {
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd) - 1, "AT+CPIN=\"%s\"", pincode.c_str());
  int ret = send_command_(cmd, nullptr);
  if (ret != 0) {
    // error info
  }
  return 0;
}

int ATCommandManager::setNewPinCode(const std::string &oldcode,
                                    const std::string &newcode) {
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+CPIN=\"%s\",\"%s\"", oldcode.c_str(),
           newcode.c_str());
  int ret = send_command_(cmd, nullptr);
  if (ret != 0) {
    // error info
  }
  return 0;
}

/* Ehrpd enable off is needed by pppd dial,
 * but quickly dial do not need it*/
bool ATCommandManager::setEhrpd() {
  int rc;
  char *line = nullptr;
  ATResponse *rsp = nullptr;
  char ret = -1;
  char cmd[kMaxATCmdLen] = {0};

  rc = send_command_singleline_("AT+EHRPDENABLE?", "+EHRPDENABLE:", &rsp);
  if (rc < 0 || rsp->success_ == 0) {
    // assume radio is off
    free_resp_(rsp);
    return false;
  }
  line = rsp->intermediates_->line_;

  rc = AtTokStart(&line);
  if (rc < 0) {
    free_resp_(rsp);
    return false;
  }

  rc = AtTokNextBool(&line, &ret);
  if (rc < 0) {
    free_resp_(rsp);
    return false;
  }

  free_resp_(rsp);

  uint32_t enehrpd = 0;
  /*ret equal 1(true) represent is on*/
  if ((uint32_t)ret == enehrpd) {
    free_resp_(rsp);
    return 0;
  }
  snprintf(cmd, sizeof(cmd) - 1, "AT+EHRPDENABLE=%d", enehrpd);
  rc = send_command_(cmd, &rsp);
  if (rc < 0 || rsp->success_ == 0) {
    free_resp_(rsp);
    return false;
  }

  free_resp_(rsp);
  return true;
}

int ATCommandManager::getNetSearchType() {
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+MODODREX?");

  // huawei 3g
  snprintf(cmd, sizeof(cmd), "AT^ANTMODE");
  // u7500
  snprintf(cmd, sizeof(cmd), "AT+MODODR?");
  // c7500
  snprintf(cmd, sizeof(cmd), "AT^PREFMODE?");
  return true;
}

int ATCommandManager::setNetSearchType() {
  int ret = -1;

  // if(modem == MODEM_HUAWEI)
  //     rc =
  // at_send_command("AT^SYSCFGEX=\"030201\",3FFFFFFF,1,2,7FFFFFFFFFFFFFFF,,",
  // nullptr);
  // else if(modem == MODEM_LONGSUNG)
  //     rc = at_send_command("AT+MODODREX=2", nullptr);

  return ret;
}

/* 信号实时获取,不采用同步获取的方式 */
int ATCommandManager::setSignalNotify() {
  int ret;

  ret = send_command_("AT+CSQ", nullptr);
  if (ret != 0) {
    ret = send_command_("AT+CCSQ", nullptr);
    if (ret != 0) {
      return -1;
    }
  }
  return ret;
}

int ATCommandManager::getSignal(void) {
  // 8300c
  int ret = send_command_("AT+SIGNALIND=1", nullptr);
  if (ret != 0) {
  }
  return 0;
  //"+SIGNALIND:99, rssi,ber " //"level:UNKNOWN"
  //"+SIGNALIND:0, rssi,ber" //"level:0"
}

//置支持热插拔功能
int ATCommandManager::setWapen(void) {
  // 8300c
  // AT+ SIMSWAPEN?
  send_command_("AT+SIMSWAPEN=1", nullptr);
  return 0;
}

//获取小区信息,信噪比等信息
int ATCommandManager::getLSCellInfo(void) {
  int rc = -1;

  // 8300c
  rc = send_command_("AT+LSCELLINFO", nullptr);
  if (rc != 0) {
  }
  return 0;
  /*
  ====AT+LSCELLINFO
  LTE SERV CELL INFO:
          EARFCN:39148 GCELLID:190041608 TAC:22546 MCC:460 MNC:00 DLBW:5 ULBW:5
                  SINR:25.6 CAT:3 BAND:40 PCI:240  RSRP:-84  RSRQ:-3 RSSI:-60
  LTE INTRA INFO:
          PCI[0]:240 RSRQ[0]:-3 RSRP[0]:-84 RSSI[0]:-60 RXLEV[0]:43
  LTE INTER INFO:
          EARFCN[0]:38950
                  PCI:44 RSRQ:-4 RSRP:-82 RSSI:-58
          EARFCN[1]:1250

  OK
  ====
  */
}

int ATCommandManager::getClccInfo(void) {
  int rc = -1;

  // 8300c
  rc = send_command_("AT+CLCC", nullptr);
  if (rc != 0) {
  }
  return 0;
  /*
  ====
  +CLIP: "18969188036",161,,,,0
  ====
  ====
  RING
  +CLIP: "18969188036",161,,,,0
  ====
  ====ATA
  ====at+clcc
  +CLCC: 1,0,0,1,0,"",128
  +CLCC: 2,1,0,0,0,"18969188036",161
  OK
  ====
  ====
  +DISC: 2,1,0,16,"18969188036",161
  ====
  */
}

int ATCommandManager::setDtmf(void) {
  int rc = -1;

  // 8300c
  /*
   * DTMF 由高频群和低频群组成,高低频群各包含4个频率.
   * 一个高频信号和一个低频信号叠加组成一个组合信号,代表一个数字.
   * DTMF信令有16个编码, 对于设备来说是主叫输入，被叫输出.
   *  ====
   *	+DTMF: 0
   *	====
   */
  rc = send_command_("AT+LSHTONEDET=%d", nullptr);
  if (rc != 0) {
  }
  return 0;
}

int ATCommandManager::setAudio(void) {
  int rc = -1;
  /* 龙尚模块
   * 目前音频只支持PCM语音,MASTER mode,CLK 1024KHZ,SYNC 8KHZ,16bit linear.
   * 现在的9x15 平台开始,语音参数是通过一个类似于efs 的acdb 数据库配置的,
   * 如果需要修改修改这些参数，需要利用高通的QACT 工具,修改acdb 配置,
   * 利用adb 将acdb上传到ap 侧,重启生效.
   */

  // 8300c
  //启动pcm并加载acdb,速度慢,需要18s 后才有pcm clock 输出,音质优化。
  rc = send_command_("AT+SYSCMD=start_pcm acdb", nullptr);
  if (rc != 0) {
  }
  rc = send_command_("AT+SYSCMD=start_pcm volume 33", nullptr);
  if (rc != 0) {
  }
  return 0;
}

int ATCommandManager::answerCall(void) {
  int rc = -1;

  // 8300c
  rc = send_command_("ATA", nullptr);
  if (rc != 0) {
  }
  return 0;
}

int ATCommandManager::setCallInfo(void) {
  int rc = -1;

  //+CLIP:02150809688,129,,,,0   模块来电,号码是02150809688

  // 8300c
  rc = send_command_("AT+CLIP=%d", nullptr);
  if (rc != 0) {
  }
  return 0;
}

int ATCommandManager::getCallInfo(void) {
  int rc = -1;

  // 8300c
  rc = send_command_("AT+CLIP", nullptr);
  if (rc != 0) {
  }
  return 0;
}

int ATCommandManager::setTTS(void) {
  // 8300c
  /*
  U8300C U8300W
  at+lshtts=0,"start codehere"
  at+lshtts=2,"9F995C1A79D16280FF0C00770065006C0063006F006D0065"
  音量调节:
  AT+LSHTTSCONF=?
  AT+LSHTTSCONF=3  0-3 2默认音量

  TTS 停止播报
  AT+LSHTTSSTP 语音播报时，将停止语音播报
  */

  // HUAWEI
  // AT^TTSCFG=<op>,<value>
  // AT^TTS
  //^AUDEND

  return 0;
}

/* AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?;
 * AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?;
 * AT+QCIMI
 */
int ATCommandManager::getOperator() {
#if 0
    int rc;
    char resp[16] = { 0 };
    char *line;

    rc = send_command_singleline_("AT+CIMI", "+CIMI:", &rsp);
    if (rc != 0) {
        rc = send_command_singleline_("AT+QCIMI", "+QCIMI:", &rsp);
        if (rc != 0) {
            goto done;
        }
    }

    if (rc < 0 || rsp->success_ == 0) {
       // assume radio is off
       goto done;
   }

   line = rsp->intermediates_->line_;
   rc = AtTokStart(&line);
   if (rc < 0) goto done;

   rc = AtTokNextStr(&line, (char **)&resp);
   if (rc < 0) goto done;

//    match_func(resp);
done:
    free_resp_(rsp);
    return rc;


    /*AT+COPS*/
    int s_mcc = 0;
    int s_mnc = 0;
    int i;
    int skip;
    ATLine *p_cur;
    char* response[3];
    rc = send_command_("AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?", "+COPS:", &p_response);
    /* we expect 3 lines here:
     * +COPS: 0,0,"T - mobile"
     * +COPS: 0,1,"TMO"
     * +COPS: 0,2,"310170"
     *  exp:
     *  +COPS: 0,0,"CHN-UNICOM",7
     *  +COPS: 0,1,"UNICOM",7
     *  +COPS: 0,2,"46001",7

     *  +COPS: 0,0,"CHINA MOBILE",7
     *  +COPS: 0,1,"CMCC",7
     *  +COPS: 0,2,"46000",7

     *  +COPS: 0,0,"CMCC",7
     *  +COPS: 0,1,"CMCC",7
     *  +COPS: 0,2,"46000",7
        
     *  +COPS: 0
     *  +COPS: 0
     *  +COPS: 0
     */
     
    for (i = 0, p_cur = rsp->intermediates_
            ; p_cur != nullptr
            ; p_cur = p_cur->p_next, i++) {

        s8 *line = p_cur->line;

        rc = at_tok_start(&line);
        if (rc < 0) goto error;

        rc = at_tok_nextint(&line, &skip);
        if (rc < 0) goto error;

        // If we're unregistered, we may just get
        // a "+COPS: 0" response
        if (!at_tok_hasmore(&line)) {
            response[i] = nullptr;
            continue;
        }

        rc = at_tok_nextint(&line, &skip);
        if (rc < 0) goto error;


        // a "+COPS: 0, n" response is also possible
        if (!at_tok_hasmore(&line)) {
            response[i] = nullptr;
            continue;
        }

        rc = at_tok_nextstr(&line, &(response[i]));
        if (rc < 0) goto error;

        //operator
        response[i];

        // Simple assumption that mcc and mnc are 3 digits each
        if (strlen(response[i]) == 6) {
            if (sscanf(response[i], "%3d%3d", &s_mcc, &s_mnc) != 2) {
                MSF_MOBILE_LOG(DBG_ERROR, "Expected mccmnc to be 6 decimal digits.");
            }
        }
    }

    if (i != 3) {
        /* expect 3 lines exactly */
        goto error;
    }
#endif
  return 0;
}

bool ATCommandManager::listAllOperators() {
  send_command_("AT+COPS=?", nullptr);
  return true;
}

bool ATCommandManager::listOperatorByNumberic() {
  send_command_("AT+COPS=3,2", nullptr);
  return true;
}

bool ATCommandManager::setAutoRegister() {
  send_command_("AT+COPS=0,0", nullptr);
  return true;
}

bool ATCommandManager::getNetMode() {
  send_command_("AT+PSRAT", nullptr);
  // huwei 4G
  send_command_("AT^SYSINFOEX", nullptr);
  // huwei 3G
  send_command_("AT^HCSQ?", nullptr);
  return true;
}

int ATCommandManager::getNet3GPP() {
  int ret;
  int response = 0;
  char *line;

  ATResponse *rsp;

  // AT+COPS=?
  //+COPS:
  //(2,"CHN-UNICOM","UNICOM","46001",7),(3,"CHN-CT","CT","46011",7),(3,"CHINA
  // MOBILE","CMCC","46000",7),,(0,1,2,3,4),(0,1,2)
  // AT+COPS?
  //+COPS: 0,2,"46001",7
  ret = send_command_singleline_("AT+COPS?", "+COPS:", &rsp);
  if (ret < 0 || rsp->success_ == 0) {
    free_resp_(rsp);
    return -1;
  }

  line = rsp->intermediates_->line_;
  ret = AtTokStart(&line);
  if (ret < 0) {
    free_resp_(rsp);
    return -1;
  }

  ret = AtTokNextInt(&line, &response);
  if (ret < 0) {
    free_resp_(rsp);
    return -1;
  }

  free_resp_(rsp);
  return 0;
}

bool ATCommandManager::getNetStatus(enum NetWorkType type) {
  switch (type) {
    case NETWORK_2G:
      send_command_("AT+CGREG?", nullptr);
      break;
    case NETWORK_3G:
      send_command_("AT+CREG?", nullptr);
      break;
    case NETWORK_4G:
      send_command_("AT+CEREG?", nullptr);
      break;
    case NETWORK_5G:

      break;
    default:
      break;
  }
  return true;
}

bool ATCommandManager::setCallerId() {
  // enum CALLER_ID_STATUS call_id = CALLER_ID_OPEN;
  int call_id = 0;
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd) - 1, "AT+CLIP=%d", call_id);

  send_command_(cmd, nullptr);
  return true;
}
bool ATCommandManager::setTeStatus() {
  send_command_("AT+CNMI=2,1,2,2,0", nullptr);
  return true;
}

bool ATCommandManager::getSMSCenter() {
  send_command_("AT+CSCA?", nullptr);
  return true;
}
bool ATCommandManager::setSMSStorageArea() {
  int area = 0;
  if (ME_AREA == area)
    send_command_("AT+CPMS=\"ME\",\"ME\",\"ME\"", nullptr);
  else if (SM_AREA == area)
    send_command_("AT+CPMS=\"SM\",\"SM\",\"SM\"", nullptr);
  return true;
}
bool ATCommandManager::setSMSFormat() {
  uint32_t sms_format = 0;
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+CMGF=%d", sms_format);
  if (AT_SUCCESS == send_command_(cmd, nullptr)) return true;

  // evdo
  memset(cmd, 0, sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "AT$QCMGF=%d", sms_format);
  if (AT_SUCCESS == send_command_(cmd, nullptr)) return true;

  return false;
}

bool ATCommandManager::hasSMS() {
  int ret = -1;

  ret = send_command_("AT+CMGD=?", nullptr);
  if (ret != 0) {
    /*Telecom evdo*/
    ret = send_command_("AT$QCMGD=?", nullptr);
    if (ret != 0) {
      return false;
    }
  }
  return true;
}

bool ATCommandManager::readSMS() {
  uint32_t index = 0;
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+CMGR=%d", index);
  snprintf(cmd, sizeof(cmd), "AT^HCMGR=%d", index);

  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::clearSMS() {
  uint32_t sms_format = 0, sms_clean_mode = 0;
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+CMGD=%d,%d", sms_format, sms_clean_mode);
  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::getAPNS(void) {
  ATLine *p_cur;
  int ret;
  char *out;
  ATResponse *rsp;

  ret = send_command_mutiline_("AT+CGDCONT?", "+CGDCONT:", &rsp);
  if (ret != 0 || rsp->success_ == 0) {
    return -1;
  }

  for (p_cur = rsp->intermediates_; p_cur != nullptr; p_cur = p_cur->next_) {
    char *line = p_cur->line_;
    int cid;

    ret = AtTokStart(&line);
    if (ret < 0) {
      free_resp_(rsp);
      return false;
    }

    ret = AtTokNextInt(&line, &cid);
    if (ret < 0) {
      free_resp_(rsp);
      return false;
    }

    // printf("cid = %d \n", cid);

    ret = AtTokNextStr(&line, &out);
    if (ret < 0) {
      free_resp_(rsp);
      return false;
    }

    // printf("type = %s \n", out);

    // APN ignored for v5
    ret = AtTokNextStr(&line, &out);
    if (ret < 0) {
      free_resp_(rsp);
      return false;
    }

    // printf("apn = %s \n", out);

    ret = AtTokNextStr(&line, &out);
    if (ret < 0) {
      free_resp_(rsp);
      return false;
    }

    // printf("addresses = %s \n", out);
  }

  free_resp_(rsp);
  return true;
}

bool ATCommandManager::clearAPNS() {
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+cgdcont=1");
  send_command_(cmd, nullptr);

  snprintf(cmd, sizeof(cmd), "AT+cgdcont=2");
  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::setAPNS() {
  char apn1[16];
  char apn2[16];

  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+cgdcont=1,\"ip\",\"%s\"", apn1);
  snprintf(cmd, sizeof(cmd), "AT+cgdcont=2,\"ip\",\"%s\"", apn2);

  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::setAuthInfo() {
  char cmd[kMaxATCmdLen] = {0};
  uint8_t verifyProto = 2;
  uint8_t szPassword[32];
  uint8_t szUsername[32];
  // LongSung: AT$QCPDPP=1,2,passwod, username
  // AT^PPPCFG=<user_name>,<password>
  snprintf(cmd, sizeof(cmd), "AT$QCPDPP=1,%d,\"%s\",\"%s\"", verifyProto,
           szPassword, szUsername);

  // Huawei: AT^AUTHDATA=<cid><Auth_type><PLMN><passwd><username>
  snprintf(cmd, sizeof(cmd), "AT^AUTHDATA=1,%d,\"\",\"%s\",\"%s\"", verifyProto,
           szPassword, szUsername);

  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::setPDPStatus(uint32_t pdpStatus) {
  char cmd[kMaxATCmdLen] = {0};
  snprintf(cmd, sizeof(cmd), "AT+CGACT=%d,1", pdpStatus);
  send_command_(cmd, nullptr);
  return true;
}

bool ATCommandManager::setQosRequestBrief(void) {
  send_command_("AT+CGEQREQ=1,3,11520,11520,11520,11520,,,,,,,3", nullptr);
  return true;
}
}  // namespace mobile
