// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "torrent/exceptions.h"

#include "download/choke_manager.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"
#include "download/download_main.h"
#include "data/file_manager.h"
#include "protocol/handshake_manager.h"
#include "data/hash_queue.h"
#include "net/listen.h"

#include "manager.h"
#include "resource_manager.h"

namespace torrent {

Manager* manager = NULL;

Manager::Manager() :
  m_downloadManager(new DownloadManager),
  m_fileManager(new FileManager),
  m_handshakeManager(new HandshakeManager),
  m_hashQueue(new HashQueue),
  m_listen(new Listen),
  m_resourceManager(new ResourceManager),

  m_ticks(0) {

  m_taskTick.set_iterator(taskScheduler.end());
  m_taskTick.set_slot(sigc::mem_fun(*this, &Manager::receive_tick));

  taskScheduler.insert(&m_taskTick, (Timer::cache()).round_seconds());

  m_listen->slot_incoming(sigc::mem_fun(*m_handshakeManager, &HandshakeManager::add_incoming));
}

Manager::~Manager() {
  taskScheduler.erase(&m_taskTick);

  m_handshakeManager->clear();
  m_downloadManager->clear();

  delete m_downloadManager;
  delete m_fileManager;
  delete m_handshakeManager;
  delete m_hashQueue;
  delete m_listen;
  delete m_resourceManager;
}

void
Manager::initialize_download(DownloadWrapper* d) {
  m_downloadManager->insert(d);
  m_resourceManager->insert(1, d->main());

  d->main()->choke_manager()->slot_choke(rak::make_mem_fn(manager->resource_manager(), &ResourceManager::receive_choke));
  d->main()->choke_manager()->slot_unchoke(rak::make_mem_fn(manager->resource_manager(), &ResourceManager::receive_unchoke));
}

void
Manager::cleanup_download(DownloadWrapper* d) {
  m_resourceManager->erase(d->main());
  m_downloadManager->erase(d);
}

void
Manager::receive_tick() {
  m_ticks++;

  if (m_ticks % 4 == 0)
    std::for_each(m_downloadManager->begin(), m_downloadManager->end(), std::mem_fun(&DownloadWrapper::receive_keepalive));

  m_resourceManager->receive_tick();

  // If you change the interval, make sure the above keepalive gets
  // triggered every 120 seconds.
  taskScheduler.insert(&m_taskTick, (Timer::cache() + 30 * 1000000).round_seconds());
}

}
