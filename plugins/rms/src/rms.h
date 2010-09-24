/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 2000-2010 Licq developers
 *
 * Licq is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Licq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Licq; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LICQRMS_H
#define LICQRMS_H

#include <list>

#include <licq/logging/pluginlogsink.h>
#include <licq/socket.h>
#include <licq/socketmanager.h>
#include <licq/userid.h>

namespace Licq
{
class Event;
class PluginSignal;
class UserEvent;
}

const unsigned short MAX_LINE_LENGTH = 1024 * 1;
const unsigned short MAX_TEXT_LENGTH = 1024 * 8;

typedef std::list<class CRMSClient*> ClientList;
typedef std::list<unsigned long> TagList;

class CLicqRMS
{
public:
  CLicqRMS(bool, unsigned short);
  ~CLicqRMS();
  int Run();
  void Shutdown();
  bool Enabled() { return m_bEnabled; }

protected:
  int m_nPipe;
  bool m_bExit, m_bEnabled;

  unsigned short m_nPort;

  Licq::TCPSocket* server;
  ClientList clients;
  Licq::PluginLogSink::Ptr myLogSink;

public:
  void ProcessPipe();
  void ProcessSignal(Licq::PluginSignal* s);
  void ProcessEvent(Licq::Event* e);
  void ProcessServer();
  void ProcessLog();

friend class CRMSClient;

};


class CRMSClient
{
public:
  CRMSClient(Licq::TCPSocket*);
  ~CRMSClient();

  int Activity();

  static Licq::SocketManager sockman;

  int Process_QUIT();
  int Process_TERM();
  int Process_INFO();
  int Process_STATUS();
  int Process_HELP();
  int Process_GROUPS();
  int Process_HISTORY();
  int Process_LIST();
  int Process_MESSAGE();
  int Process_URL();
  int Process_SMS();
  int Process_LOG();
  int Process_VIEW();
  int Process_AR();
  int Process_ADDUSER();
  int Process_REMUSER();
  int Process_SECURE();
  int Process_NOTIFY();

protected:
  Licq::TCPSocket sock;
  FILE *fs;
  TagList tags;
  unsigned short m_nState;
  char data_line[MAX_LINE_LENGTH + 1];
  char *data_arg;
  unsigned short data_line_pos;
  unsigned long m_nCheckUin;
  char *m_szCheckId;
  unsigned int myLogLevelsBitmask;
  bool m_bNotify;

  unsigned long m_nUin;
  Licq::UserId myUserId;
  char m_szText[MAX_TEXT_LENGTH + 1];
  char m_szLine[MAX_LINE_LENGTH + 1];
  unsigned short m_nTextPos;

  int StateMachine();
  int ProcessCommand();
  bool ProcessEvent(Licq::Event* e);
  bool AddLineToText();
  unsigned long GetProtocol(const char *);
  void ParseUser(const char *);
  int changeStatus(unsigned long, const char *);

  int Process_MESSAGE_text();
  int Process_URL_url();
  int Process_URL_text();
  int Process_SMS_number();
  int Process_SMS_message();
  int Process_AR_text();

  /**
   * Output a user event
   *
   * @param e User event
   * @param alias Alias of sender
   */
  void printUserEvent(const Licq::UserEvent* e, const std::string& alias);

friend class CLicqRMS;
};

extern CLicqRMS *licqRMS;


#endif
